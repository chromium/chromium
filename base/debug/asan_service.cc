// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/debug/asan_service.h"

#if defined(ADDRESS_SANITIZER)
#include <sanitizer/asan_interface.h>

#include "base/debug/task_trace.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/logging.h"
#include "base/win/windows_types.h"
#endif  // BUILDFLAG(IS_WIN)

#if defined(COMPONENT_BUILD) && BUILDFLAG(IS_WIN)
// In component builds on Windows, weak function exported by ASan have the
// `__dll` suffix. ASan itself uses the `alternatename` directive to account for
// that.
#pragma comment(linker,                                                \
                    "/alternatename:__sanitizer_report_error_summary=" \
                    "__sanitizer_report_error_summary__dll")
#pragma comment(linker,                                     \
                "/alternatename:__sanitizer_set_report_fd=" \
                "__sanitizer_set_report_fd__dll")
#endif  // defined(COMPONENT_BUILD) && BUILDFLAG(IS_WIN)

namespace base {
namespace debug {

namespace {
NO_SANITIZE("address")
void TaskTraceErrorCallback(const char* error, bool*) {
  // Use the sanitizer api to symbolize the task trace, which otherwise might
  // not symbolize properly. This also lets us format the task trace in the
  // same way as the address sanitizer backtraces, which also means that we can
  // get the stack trace symbolized with asan_symbolize.py in the cases where
  // symbolization at runtime fails.
  std::array<const void*, 4> addresses;
  size_t address_count = TaskTrace().GetAddresses(addresses);

  AsanService::GetInstance()->Log("Task trace:");
  size_t frame_index = 0;
  for (size_t i = 0; i < std::min(address_count, addresses.size()); ++i) {
    char buffer[4096] = {};
    void* address = const_cast<void*>(addresses[i]);
    __sanitizer_symbolize_pc(address, "%p %F %L", buffer, sizeof(buffer));
    for (char* ptr = buffer; *ptr != 0; ptr += strlen(ptr)) {
      AsanService::GetInstance()->Log("    #%i %s", frame_index++, ptr);
    }
  }
  AsanService::GetInstance()->Log("");
}
}  // namespace

// static
NO_SANITIZE("address")
AsanService* AsanService::GetInstance() {
  static NoDestructor<AsanService> instance;
  return instance.get();
}

void AsanService::Initialize() {
  AutoLock lock(lock_);
  if (!is_initialized_) {
#if BUILDFLAG(IS_WIN)
    if (logging::IsLoggingToFileEnabled()) {
      // Sandboxed processes cannot open files but are provided a HANDLE.
      HANDLE log_handle = logging::DuplicateLogFileHandle();
      if (log_handle) {
        // Sanitizer APIs need a HANDLE cast to void*.
        __sanitizer_set_report_fd(reinterpret_cast<void*>(log_handle));
      }
    }
#endif  // BUILDFLAG(IS_WIN)
    __asan_set_error_report_callback(ErrorReportCallback);
    error_callbacks_.push_back(TaskTraceErrorCallback);
    is_initialized_ = true;
  }
}

NO_SANITIZE("address")
void AsanService::Log(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  auto formatted_message = StringPrintV(format, ap);
  va_end(ap);

  // Despite its name, the function just prints the input to the destination
  // configured by ASan.
  __sanitizer_report_error_summary(formatted_message.c_str());
}

void AsanService::AddErrorCallback(ErrorCallback error_callback) {
  AutoLock lock(lock_);
  CHECK(is_initialized_);
  error_callbacks_.push_back(error_callback);
}

NO_SANITIZE("address")
void AsanService::RunErrorCallbacks(const char* reason) {
  ProcessId process_id = GetCurrentProcId();
  bool should_exit_cleanly = false;

  {
    // We can hold `lock_` throughout the error callbacks, since ASan doesn't
    // re-enter when handling nested errors on the same thread.
    AutoLock lock(lock_);

    Log("\n==%i==ADDITIONAL INFO", (int)process_id);
    Log("\n==%i==Note: Please include this section with the ASan report.",
        (int)process_id);
    for (const auto& error_callback : error_callbacks_) {
      error_callback(reason, &should_exit_cleanly);
    }
    Log("\n==%i==END OF ADDITIONAL INFO", (int)process_id);
  }

  if (should_exit_cleanly) {
    Log("\n==%i==EXITING", (int)process_id);
    Process::TerminateCurrentProcessImmediately(0);
  }
}

// static
NO_SANITIZE("address")
void AsanService::ErrorReportCallback(const char* reason) {
  AsanService::GetInstance()->RunErrorCallbacks(reason);
}

AsanService::AsanService() = default;

}  // namespace debug
}  // namespace base

#endif  // defined(ADDRESS_SANITIZER)
