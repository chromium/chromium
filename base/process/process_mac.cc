// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include <mach/mach.h>
#include <stddef.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <unistd.h>

#include <iterator>
#include <memory>

#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/mac/mach_logging.h"
#include "base/memory/free_deleter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

namespace {

// Enables setting the task role of every child process to
// TASK_DEFAULT_APPLICATION.
BASE_FEATURE(kMacSetDefaultTaskRole,
             "MacSetDefaultTaskRole",
             FEATURE_DISABLED_BY_DEFAULT);

// Returns the `task_role_t` of the process whose process ID is `pid`.
absl::optional<task_role_t> GetTaskCategoryPolicyRole(
    PortProvider* port_provider,
    ProcessId pid) {
  DCHECK(port_provider);

  mach_port_t task_port = port_provider->TaskForPid(pid);
  if (task_port == TASK_NULL) {
    return absl::nullopt;
  }

  task_category_policy_data_t category_policy;
  mach_msg_type_number_t task_info_count = TASK_CATEGORY_POLICY_COUNT;
  boolean_t get_default = FALSE;

  kern_return_t result =
      task_policy_get(task_port, TASK_CATEGORY_POLICY,
                      reinterpret_cast<task_policy_t>(&category_policy),
                      &task_info_count, &get_default);
  if (result != KERN_SUCCESS) {
    MACH_LOG(ERROR, result) << "task_policy_get TASK_CATEGORY_POLICY";
    return absl::nullopt;
  }
  DCHECK(!get_default);
  return category_policy.role;
}

}  // namespace

Time Process::CreationTime() const {
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, Pid()};
  size_t len = 0;
  if (sysctl(mib, std::size(mib), NULL, &len, NULL, 0) < 0)
    return Time();

  std::unique_ptr<struct kinfo_proc, base::FreeDeleter> proc(
      static_cast<struct kinfo_proc*>(malloc(len)));
  if (sysctl(mib, std::size(mib), proc.get(), &len, NULL, 0) < 0)
    return Time();
  return Time::FromTimeVal(proc->kp_proc.p_un.__p_starttime);
}

bool Process::CanBackgroundProcesses() {
  return true;
}

bool Process::IsProcessBackgrounded(PortProvider* port_provider) const {
  DCHECK(IsValid());
  DCHECK(port_provider);

  // A process is backgrounded if the role is explicitly
  // TASK_BACKGROUND_APPLICATION (as opposed to not being
  // TASK_FOREGROUND_APPLICATION).
  absl::optional<task_role_t> task_role =
      GetTaskCategoryPolicyRole(port_provider, Pid());
  return task_role && *task_role == TASK_BACKGROUND_APPLICATION;
}

bool Process::SetProcessBackgrounded(PortProvider* port_provider,
                                     bool background) {
  DCHECK(IsValid());
  DCHECK(port_provider);

  if (!CanBackgroundProcesses()) {
    return false;
  }

  mach_port_t task_port = port_provider->TaskForPid(Pid());
  if (task_port == TASK_NULL)
    return false;

  absl::optional<task_role_t> current_role =
      GetTaskCategoryPolicyRole(port_provider, Pid());
  if (!current_role) {
    return false;
  }

  if ((background && *current_role == TASK_BACKGROUND_APPLICATION) ||
      (!background && *current_role == TASK_FOREGROUND_APPLICATION)) {
    return true;
  }

  task_category_policy category_policy;
  category_policy.role =
      background ? TASK_BACKGROUND_APPLICATION : TASK_FOREGROUND_APPLICATION;
  kern_return_t result =
      task_policy_set(task_port, TASK_CATEGORY_POLICY,
                      reinterpret_cast<task_policy_t>(&category_policy),
                      TASK_CATEGORY_POLICY_COUNT);

  if (result != KERN_SUCCESS) {
    MACH_LOG(ERROR, result) << "task_policy_set TASK_CATEGORY_POLICY";
    return false;
  }

  return true;
}

// static
void Process::SetCurrentTaskDefaultRole() {
  if (!base::FeatureList::IsEnabled(kMacSetDefaultTaskRole)) {
    return;
  }

  task_category_policy category_policy;
  category_policy.role = TASK_DEFAULT_APPLICATION;
  task_policy_set(mach_task_self(), TASK_CATEGORY_POLICY,
                  reinterpret_cast<task_policy_t>(&category_policy),
                  TASK_CATEGORY_POLICY_COUNT);
}

}  // namespace base
