// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"

#include <lib/zx/process.h>
#include <stdlib.h>
#include <unistd.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>

#include "base/debug/alias.h"

namespace base {
namespace debug {

bool BeingDebugged() {
  zx_info_process_t info = {};
  // Ignore failures. The 0-initialization above will result in "false" for
  // error cases.
  zx::process::self()->get_info(ZX_INFO_PROCESS, &info, sizeof(info),
                                nullptr, nullptr);
  return (info.flags & ZX_INFO_PROCESS_FLAG_DEBUGGER_ATTACHED) != 0;
}

void BreakDebuggerAsyncSafe() {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.

  // Linker's ICF feature may merge this function with other functions with the
  // same definition (e.g. any function whose sole job is to call abort()) and
  // it may confuse the crash report processing system. http://crbug.com/508489
  static int static_variable_to_make_this_function_unique = 0;
  Alias(&static_variable_to_make_this_function_unique);

  abort();
}

void VerifyDebugger() {}

}  // namespace debug
}  // namespace base
