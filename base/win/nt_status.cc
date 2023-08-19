// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/nt_status.h"

#include <windows.h>

extern "C" NTSTATUS WINAPI RtlGetLastNtStatus();

namespace base {
namespace win {

NTSTATUS GetLastNtStatus() {
  // This is equivalent to calling NtCurrentTeb() and extracting
  // LastStatusValue from the returned _TEB structure, except that the public
  // _TEB struct definition does not actually specify the location of the
  // LastStatusValue field. We avoid depending on such a definition by
  // internally using RtlGetLastNtStatus() from ntdll.dll instead.
  return ::RtlGetLastNtStatus();
}

}  // namespace win
}  // namespace base
