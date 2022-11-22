// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"

#include <windows.h>

#include <tlhelp32.h>

#include <ostream>

#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"

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

ProcessId GetParentProcessId(ProcessHandle process) {
  ProcessId child_pid = GetProcId(process);
  PROCESSENTRY32 process_entry;
      process_entry.dwSize = sizeof(PROCESSENTRY32);

  win::ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (snapshot.is_valid() && Process32First(snapshot.get(), &process_entry)) {
    do {
      if (process_entry.th32ProcessID == child_pid)
        return process_entry.th32ParentProcessID;
    } while (Process32Next(snapshot.get(), &process_entry));
  }

  // TODO(zijiehe): To match other platforms, -1 (UINT32_MAX) should be returned
  // if |child_id| cannot be found in the |snapshot|.
  return 0u;
}

}  // namespace base
