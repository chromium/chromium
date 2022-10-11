// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/process_startup_helper.h"

#include <crtdbg.h>
#include <new.h>

#include "base/base_switches.h"
#include "base/command_line.h"

namespace {

// Handlers for invalid parameter and pure call. They generate a breakpoint to
// tell breakpad that it needs to dump the process.
// These functions should be written to be unique in order to avoid confusing
// call stacks from /OPT:ICF function folding. Printing a unique message or
// returning a unique value will do this. Note that for best results they need
// to be unique from *all* functions in Chrome.
void InvalidParameter(const wchar_t* expression,
                      const wchar_t* function,
                      const wchar_t* file,
                      unsigned int line,
                      uintptr_t reserved) {
  __debugbreak();
  // Use a different exit code from PureCall to avoid COMDAT folding.
  _exit(1);
}

void PureCall() {
  __debugbreak();
  // Use a different exit code from InvalidParameter to avoid COMDAT folding.
  _exit(2);
}

}  // namespace

namespace base {
namespace win {

// Register the invalid param handler and pure call handler to be able to
// notify breakpad when it happens.
void RegisterInvalidParamHandler() {
  _set_invalid_parameter_handler(InvalidParameter);
  _set_purecall_handler(PureCall);
}

void SetupCRT(const CommandLine& command_line) {
#if defined(_CRTDBG_MAP_ALLOC)
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#else
  if (!command_line.HasSwitch(switches::kDisableBreakpad)) {
    _CrtSetReportMode(_CRT_ASSERT, 0);
  }
#endif
}

}  // namespace win
}  // namespace base
