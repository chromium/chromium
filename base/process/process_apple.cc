// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include <sys/sysctl.h>
#include <sys/time.h>
#include <unistd.h>

#include <iterator>

#include "base/check.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"

namespace base {

Time Process::CreationTime() const {
  ProcessId pid = is_current() ? GetCurrentProcId() : Pid();
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};
  struct kinfo_proc process_info;
  size_t size = sizeof(process_info);
  if (sysctl(mib, std::size(mib), &process_info, &size, nullptr, 0) < 0) {
    DPCHECK(false);
    return Time();
  }
  if (size < sizeof(process_info)) {
    return Time();
  }
  return Time::FromTimeVal(process_info.kp_proc.p_un.__p_starttime);
}

}  // namespace base
