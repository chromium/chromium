// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/nt_status.h"

#include "base/check.h"

#include <windows.h>
#include <winternl.h>

using GetLastNtStatusFn = NTSTATUS NTAPI (*)();

constexpr const wchar_t kNtDllName[] = L"ntdll.dll";
constexpr const char kLastStatusFnName[] = "RtlGetLastNtStatus";

namespace base {
namespace win {

NTSTATUS GetLastNtStatus() {
  // This is equivalent to calling NtCurrentTeb() and extracting
  // LastStatusValue from the returned _TEB structure, except that the public
  // _TEB struct definition does not actually specify the location of the
  // LastStatusValue field. We avoid depending on such a definition by
  // internally using RtGetLastNtStatus() from ntdll.dll instead.
  static auto* get_last_nt_status = reinterpret_cast<GetLastNtStatusFn>(
      ::GetProcAddress(::GetModuleHandle(kNtDllName), kLastStatusFnName));
  return get_last_nt_status();
}

}  // namespace win
}  // namespace base
