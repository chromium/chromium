// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"

#include <windows.h>

#include <winternl.h>

#include <ostream>

#include "base/check.h"

namespace base {

ProcessId GetCurrentProcId() {
  return ::GetCurrentProcessId();
}

ProcessHandle GetCurrentProcessHandle() {
  return ::GetCurrentProcess();
}

ProcessId GetProcId(ProcessHandle process) {
  if (process == base::kNullProcessHandle)
    return 0;
  // This returns 0 if we have insufficient rights to query the process handle.
  // Invalid handles or non-process handles will cause a hard failure.
  ProcessId result = GetProcessId(process);
  CHECK(result != 0 || GetLastError() != ERROR_INVALID_HANDLE)
      << "process handle = " << process;
  return result;
}

// Local definition to include InheritedFromUniqueProcessId which contains a
// unique identifier for the parent process. See documentation at:
// https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryinformationprocess
typedef struct _PROCESS_BASIC_INFORMATION {
  PVOID Reserved1;
  PPEB PebBaseAddress;
  PVOID Reserved2[2];
  ULONG_PTR UniqueProcessId;
  ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION;

ProcessId GetParentProcessId(ProcessHandle process) {
  HINSTANCE ntdll = GetModuleHandle(L"ntdll.dll");
  decltype(NtQueryInformationProcess)* nt_query_information_process =
      reinterpret_cast<decltype(NtQueryInformationProcess)*>(
          GetProcAddress(ntdll, "NtQueryInformationProcess"));
  if (!nt_query_information_process) {
    return 0u;
  }

  PROCESS_BASIC_INFORMATION pbi = {};
  // TODO(zijiehe): To match other platforms, -1 (UINT32_MAX) should be returned
  // if the parent process id cannot be found.
  ProcessId pid = 0u;
  if (NT_SUCCESS(nt_query_information_process(process, ProcessBasicInformation,
                                              &pbi, sizeof(pbi), nullptr))) {
    pid = static_cast<ProcessId>(pbi.InheritedFromUniqueProcessId);
  }

  return pid;
}

}  // namespace base
