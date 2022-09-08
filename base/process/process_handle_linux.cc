// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"

#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/internal_linux.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_AIX)
#include "base/process/internal_aix.h"
#endif

namespace base {

ProcessId GetParentProcessId(ProcessHandle process) {
  ProcessId pid =
#if BUILDFLAG(IS_AIX)
      internalAIX::ReadProcStatsAndGetFieldAsInt64(process,
                                                   internalAIX::VM_PPID);
#else
      checked_cast<ProcessId>(internal::ReadProcStatsAndGetFieldAsInt64(
          process, internal::VM_PPID));
#endif
  // TODO(zijiehe): Returns 0 if |process| does not have a parent process.
  if (pid)
    return pid;
  return -1;
}

FilePath GetProcessExecutablePath(ProcessHandle process) {
  FilePath stat_file = internal::GetProcPidDir(process).Append("exe");
  FilePath exe_name;
  if (!ReadSymbolicLink(stat_file, &exe_name)) {
    // No such process.  Happens frequently in e.g. TerminateAllChromeProcesses
    return FilePath();
  }
  return exe_name;
}

}  // namespace base
