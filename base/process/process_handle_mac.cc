// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"

#include <stddef.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_IOS)
#include "base/ios/sim_header_shims.h"
#else
#include <libproc.h>
#endif

namespace base {

ProcessId GetParentProcessId(ProcessHandle process) {
  struct kinfo_proc info;
  size_t length = sizeof(struct kinfo_proc);
  int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, process };
  if (sysctl(mib, 4, &info, &length, NULL, 0) < 0) {
    DPLOG(ERROR) << "sysctl";
    return -1;
  }
  if (length == 0)
    return -1;
  return info.kp_eproc.e_ppid;
}

FilePath GetProcessExecutablePath(ProcessHandle process) {
  char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
  if (!proc_pidpath(process, pathbuf, sizeof(pathbuf)))
    return FilePath();

  return FilePath(pathbuf);
}

}  // namespace base
