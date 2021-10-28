#include <hal/video.h>
#include <hal/xbox.h>
#include <nxdk/mount.h>
#include <pbkit/pbkit.h>
#include <windows.h>

#include <string>

#define OUTPUT_FILE "E:\\cpuid.txt"

static DWORD eax_values[] = {
    0x0,        0x1,        0x2,        0x3,        0x4,        0x5,
    0x06,       0x07,       0x09,       0x0A,       0x0B,       0x1F,
    0x0D,       0x14,       0x40000000, 0x40000001, 0x80000000, 0x80000001,
    0x80000002, 0x80000003, 0x80000004, 0x80000005, 0x80000006, 0x80000007,
    0x80000008, 0x8000000A, 0x8000001D, 0x8000001E, 0xC0000000, 0xC0000001,
    0xC0000002, 0xC0000003, 0xC0000004, 0x8000001F,
};

static void display_screen(const char *text) {
  pb_wait_for_vbl();
  pb_target_back_buffer();
  pb_reset();
  pb_fill(0, 0, 640, 480, 0xFF3E003E);
  pb_erase_text_screen();

  pb_print(text);

  pb_draw_text_screen();
  while (pb_busy())
    ;
  while (pb_finished())
    ;
}

static BOOL dump_cpuid_results(const char *file_path) {
  HANDLE file = CreateFile(file_path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    DbgPrint("Failed to create output file.");
    return FALSE;
  }

  BOOL ret = TRUE;
  char output_buffer[256] = {0};

  int num_modes = sizeof(eax_values) / sizeof(eax_values[0]);
  for (auto i = 0; i < num_modes; ++i) {
    auto mode = eax_values[i];

    DWORD EAX = 0xFFFFFFFF;
    DWORD EBX = 0xFFFFFFFF;
    DWORD ECX = 0xFFFFFFFF;
    DWORD EDX = 0xFFFFFFFF;

    __asm volatile(
        "mov %[mode_select], %%eax\n\t"
        "cpuid\n\t"
        "mov %%eax, %[EAX]\n\t"
        "mov %%ebx, %[EBX]\n\t"
        "mov %%ecx, %[ECX]\n\t"
        "mov %%edx, %[EDX]\n\t"
        : [EAX] "=rm"(EAX), [EBX] "=rm"(EBX), [ECX] "=rm"(ECX), [EDX] "=rm"(EDX)
        : [mode_select] "r"(mode)
        : "%eax", "%ebx", "%ecx", "%edx");

    snprintf(output_buffer, 255,
             "MODE=0x%08X, EAX=0x%08X, EBX=0x%08X, ECX=0x%08X, EDX=0x%08X\n",
             mode, EAX, EBX, ECX, EDX);
    DbgPrint(output_buffer);

    DWORD buffer_len = strlen(output_buffer);
    DWORD bytes_written;
    if (!WriteFile(file, output_buffer, buffer_len, &bytes_written, nullptr)) {
      snprintf(output_buffer, 255, "WriteFile failed: 0x%X", GetLastError());
      DbgPrint(output_buffer);
      ret = FALSE;
      goto cleanup;
    }

    if (bytes_written != buffer_len) {
      snprintf(output_buffer, 255, "Partial write failure: wrote %d of %d",
               bytes_written, buffer_len);
      DbgPrint(output_buffer);
      ret = FALSE;
      goto cleanup;
    }
  }

cleanup:
  CloseHandle(file);
  return ret;
}

int main() {
  XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
  BOOL pbk_started = pb_init() == 0;
  if (!pbk_started) {
    DbgPrint("pbkit init failed\n");
    return 1;
  }
  pb_show_front_screen();

  if (!nxMountDrive('E', R"(\Device\Harddisk0\Partition1)")) {
    DbgPrint("Failed to mount E:");

    return ERROR_GEN_FAILURE;
  }

  display_screen("Dumping `cpuid` invocation results to '" OUTPUT_FILE "'\n");

  if (dump_cpuid_results(OUTPUT_FILE)) {
    display_screen(
        "`cpuid` invocation results are in E:\\cpuid.txt\nSleeping 10 seconds "
        "and rebooting...");
  } else {
    display_screen(
        "Failed to dump `cpuid` invocation results.\nSleeping 10 seconds and "
        "rebooting...");
  }

  Sleep(10 * 1000);

  if (pbk_started) {
    pb_kill();
  }

  XReboot();

  return 0;
}
