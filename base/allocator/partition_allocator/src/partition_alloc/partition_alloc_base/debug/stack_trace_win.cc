// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/debug/stack_trace.h"

#include <windows.h>

#include <psapi.h>

#include <algorithm>

#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/process/process_handle.h"
#include "partition_alloc/partition_alloc_base/strings/safe_sprintf.h"

namespace partition_alloc::internal::base::debug {

namespace {

void PrintStackTraceInternal(const void** trace, size_t count) {
  HANDLE process_handle = OpenProcess(
      PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcId());
  if (!process_handle) {
    return;
  }

  constexpr size_t kMaxTraces = 32u;
  count = std::max(count, kMaxTraces);
  bool is_output_trace[kMaxTraces];
  for (size_t i = 0; i < count; ++i) {
    is_output_trace[i] = false;
  }
  DWORD bytes_required = 0;
  if (EnumProcessModules(process_handle, nullptr, 0, &bytes_required)) {
    HMODULE* module_array = nullptr;
    LPBYTE module_array_bytes = nullptr;

    if (bytes_required) {
      module_array_bytes = (LPBYTE)LocalAlloc(LPTR, bytes_required);
      if (module_array_bytes) {
        unsigned int module_count = bytes_required / sizeof(HMODULE);
        module_array = reinterpret_cast<HMODULE*>(module_array_bytes);

        if (EnumProcessModules(process_handle, module_array, bytes_required,
                               &bytes_required)) {
          for (unsigned i = 0; i < module_count; ++i) {
            MODULEINFO info;
            if (GetModuleInformation(process_handle, module_array[i], &info,
                                     sizeof(info))) {
              char module_name[MAX_PATH + 1];
              bool module_name_checked = false;
              for (unsigned j = 0; j < count; j++) {
                uintptr_t base_of_dll =
                    reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
                uintptr_t address = reinterpret_cast<uintptr_t>(trace[j]);
                if (base_of_dll <= address &&
                    address < base_of_dll + info.SizeOfImage) {
                  if (!module_name_checked) {
                    size_t module_name_length = GetModuleFileNameExA(
                        process_handle, module_array[i], module_name,
                        sizeof(module_name) - 1);
                    module_name[module_name_length] = '\0';
                    module_name_checked = true;
                  }
                  // llvm-symbolizer needs --relative-address to symbolize the
                  // "address - base_of_dll".
                  char buffer[256];
                  strings::SafeSPrintf(buffer, "#%d 0x%x (%s+0x%x)\n", j,
                                       address, module_name,
                                       address - base_of_dll);
                  PA_RAW_LOG(INFO, buffer);
                  is_output_trace[j] = true;
                }
              }
            }
          }
        }
        LocalFree(module_array_bytes);
      }
    }
  }

  for (size_t i = 0; i < count; ++i) {
    if (is_output_trace[i]) {
      continue;
    }
    char buffer[256];
    strings::SafeSPrintf(buffer, "#%d 0x%x <unknown>\n", i,
                         reinterpret_cast<uintptr_t>(trace[i]));
    PA_RAW_LOG(INFO, buffer);
  }

  CloseHandle(process_handle);
}

}  // namespace

PA_NOINLINE size_t CollectStackTrace(const void** trace, size_t count) {
  // When walking our own stack, use CaptureStackBackTrace().
  return CaptureStackBackTrace(0, count, const_cast<void**>(trace), NULL);
}

void PrintStackTrace(const void** trace, size_t count) {
  PrintStackTraceInternal(trace, count);
}

}  // namespace partition_alloc::internal::base::debug
