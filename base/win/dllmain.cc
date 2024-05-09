// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/compiler_specific.h"
#include "base/win/win_util.h"

// Custom crash code to get a unique entry in crash reports.
NOINLINE static void CrashOnProcessDetach() {
  *static_cast<volatile int*>(nullptr) = 0x356;
}

// Make DllMain call the listed callbacks.  This way any third parties that are
// linked in will also be called.
BOOL WINAPI DllMain(PVOID h, DWORD reason, PVOID reserved) {
  if (DLL_PROCESS_DETACH == reason && base::win::ShouldCrashOnProcessDetach())
    CrashOnProcessDetach();
  return true;
}
