// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/delay_load_failure_hook_win.h"

// windows.h needs to be included before delayimp.h.
#include <windows.h>

#include <delayimp.h>

#include "chrome/common/win/delay_load_failure_support.h"

namespace chrome {

namespace {

bool g_hooks_enabled = true;

// Delay load failure hook that generates a crash report. By default a failure
// to delay load will trigger an exception handled by the delay load runtime and
// this won't generate a crash report.
FARPROC WINAPI DelayLoadFailureHookEXE(unsigned reason,
                                       DelayLoadInfo* dll_info) {
  if (!g_hooks_enabled)
    return 0;

  return HandleDelayLoadFailureCommon(reason, dll_info);
}

}  // namespace

void DisableDelayLoadFailureHooksForMainExecutable() {
  g_hooks_enabled = false;
}

}  // namespace chrome

// Set the delay load failure hook to the function above.
//
// The |__pfnDliFailureHook2| failure notification hook gets called
// automatically by the delay load runtime in case of failure, see
// https://docs.microsoft.com/en-us/cpp/build/reference/failure-hooks?view=vs-2019
// for more information about this.
extern "C" const PfnDliHook __pfnDliFailureHook2 =
    chrome::DelayLoadFailureHookEXE;
