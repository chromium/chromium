// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/kill.h"

#include <zircon/syscalls.h>

#include "base/logging.h"
#include "base/process/process_iterator.h"
#include "base/threading/platform_thread.h"

namespace base {

TerminationStatus GetTerminationStatus(ProcessHandle handle, int* exit_code) {
  DCHECK(exit_code);

  zx_info_process_t process_info;
  zx_status_t status =
      zx_object_get_info(handle, ZX_INFO_PROCESS, &process_info,
                         sizeof(process_info), nullptr, nullptr);
  if (status != ZX_OK) {
    DLOG(ERROR) << "unable to get termination status for " << handle;
    *exit_code = 0;
    return TERMINATION_STATUS_NORMAL_TERMINATION;
  }
  if ((process_info.flags & ZX_INFO_PROCESS_FLAG_STARTED) == 0) {
    *exit_code = 0;
    return TERMINATION_STATUS_LAUNCH_FAILED;
  }
  if ((process_info.flags & ZX_INFO_PROCESS_FLAG_EXITED) == 0) {
    *exit_code = 0;
    return TERMINATION_STATUS_STILL_RUNNING;
  }

  *exit_code = static_cast<int>(process_info.return_code);
  switch (process_info.return_code) {
    case 0:
      return TERMINATION_STATUS_NORMAL_TERMINATION;
    case ZX_TASK_RETCODE_SYSCALL_KILL:
    case ZX_TASK_RETCODE_CRITICAL_PROCESS_KILL:
    case ZX_TASK_RETCODE_POLICY_KILL:
    case ZX_TASK_RETCODE_VDSO_KILL:
      return TERMINATION_STATUS_PROCESS_WAS_KILLED;
    case ZX_TASK_RETCODE_OOM_KILL:
      return TERMINATION_STATUS_OOM;
    case ZX_TASK_RETCODE_EXCEPTION_KILL:
      return TERMINATION_STATUS_PROCESS_CRASHED;
    default:
      return TERMINATION_STATUS_ABNORMAL_TERMINATION;
  }
}

}  // namespace base
