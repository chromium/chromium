// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"

#include <limits.h>
#include <stddef.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>

namespace base {

ProcessId GetParentProcessId(ProcessHandle process) {
  struct kinfo_proc info;
  size_t length;
  int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, process };

  if (sysctl(mib, std::size(mib), &info, &length, NULL, 0) < 0)
    return -1;

  return info.ki_ppid;
}

FilePath GetProcessExecutablePath(ProcessHandle process) {
  char pathname[PATH_MAX];
  size_t length;
  int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, process };

  length = sizeof(pathname);

  if (sysctl(mib, std::size(mib), pathname, &length, NULL, 0) < 0 ||
      length == 0) {
    return FilePath();
  }

  return FilePath(std::string(pathname));
}

}  // namespace base
