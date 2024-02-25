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
#include <optional>
#include <utility>

#include "base/apple/mach_logging.h"
#include "base/feature_list.h"
#include "base/memory/free_deleter.h"

namespace base {

namespace {

// Enables setting the task role of every child process to
// TASK_DEFAULT_APPLICATION.
BASE_FEATURE(kMacSetDefaultTaskRole,
             "MacSetDefaultTaskRole",
             FEATURE_ENABLED_BY_DEFAULT);

// Returns the `task_role_t` of the process whose task port is `task_port`.
std::optional<task_role_t> GetTaskCategoryPolicyRole(mach_port_t task_port) {
  task_category_policy_data_t category_policy;
  mach_msg_type_number_t task_info_count = TASK_CATEGORY_POLICY_COUNT;
  boolean_t get_default = FALSE;

  kern_return_t result =
      task_policy_get(task_port, TASK_CATEGORY_POLICY,
                      reinterpret_cast<task_policy_t>(&category_policy),
                      &task_info_count, &get_default);
  if (result != KERN_SUCCESS) {
    MACH_LOG(ERROR, result) << "task_policy_get TASK_CATEGORY_POLICY";
    return std::nullopt;
  }
  CHECK(!get_default);
  return category_policy.role;
}

// Sets the task role for `task_port`.
bool SetTaskCategoryPolicy(mach_port_t task_port, task_role_t task_role) {
  task_category_policy task_category_policy{.role = task_role};
  kern_return_t result =
      task_policy_set(task_port, TASK_CATEGORY_POLICY,
                      reinterpret_cast<task_policy_t>(&task_category_policy),
                      TASK_CATEGORY_POLICY_COUNT);
  if (result != KERN_SUCCESS) {
    MACH_LOG(ERROR, result) << "task_policy_set TASK_CATEGORY_POLICY";
    return false;
  }
  return true;
}

// Taken from task_policy_private.h.
struct task_suppression_policy {
  integer_t active;
  integer_t lowpri_cpu;
  integer_t timer_throttle;
  integer_t disk_throttle;
  integer_t cpu_limit;
  integer_t suspend;
  integer_t throughput_qos;
  integer_t suppressed_cpu;
  integer_t background_sockets;
  integer_t reserved[7];
};

// Taken from task_policy_private.h.
#define TASK_SUPPRESSION_POLICY_COUNT                                \
  ((mach_msg_type_number_t)(sizeof(struct task_suppression_policy) / \
                            sizeof(integer_t)))

// Activates or deactivates the suppression policy to match the effect of App
// Nap.
bool SetTaskSuppressionPolicy(mach_port_t task_port, bool activate) {
  task_suppression_policy suppression_policy = {
      .active = activate,
      .lowpri_cpu = activate,
      .timer_throttle =
          activate ? LATENCY_QOS_TIER_5 : LATENCY_QOS_TIER_UNSPECIFIED,
      .disk_throttle = activate,
      .cpu_limit = 0,                                    /* unused */
      .suspend = false,                                  /* unused */
      .throughput_qos = THROUGHPUT_QOS_TIER_UNSPECIFIED, /* unused */
      .suppressed_cpu = activate,
      .background_sockets = activate,
  };
  kern_return_t result =
      task_policy_set(task_port, TASK_SUPPRESSION_POLICY,
                      reinterpret_cast<task_policy_t>(&suppression_policy),
                      TASK_SUPPRESSION_POLICY_COUNT);
  if (result != KERN_SUCCESS) {
    MACH_LOG(ERROR, result) << "task_policy_set TASK_SUPPRESSION_POLICY";
    return false;
  }
  return true;
}

// Returns true if the task suppression policy is active for `task_port`.
bool IsTaskSuppressionPolicyActive(mach_port_t task_port) {
  task_suppression_policy suppression_policy = {
      .active = false,
  };

  mach_msg_type_number_t task_info_count = TASK_SUPPRESSION_POLICY_COUNT;
  boolean_t get_default = FALSE;

  kern_return_t result =
      task_policy_get(task_port, TASK_SUPPRESSION_POLICY,
                      reinterpret_cast<task_policy_t>(&suppression_policy),
                      &task_info_count, &get_default);
  if (result != KERN_SUCCESS) {
    MACH_LOG(ERROR, result) << "task_policy_get TASK_SUPPRESSION_POLICY";
    return false;
  }
  CHECK(!get_default);

  // Only check the `active` property as it is sufficient to discern the state,
  // even though other properties could be used.
  return suppression_policy.active;
}

// Sets the task role and the suppression policy for `task_port`.
bool SetPriorityImpl(mach_port_t task_port,
                     task_role_t task_role,
                     bool activate_suppression_policy) {
  // Do both operations, even if the first one fails.
  bool succeeded = SetTaskCategoryPolicy(task_port, task_role);
  succeeded &= SetTaskSuppressionPolicy(task_port, activate_suppression_policy);
  return succeeded;
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

bool Process::CanSetPriority() {
  return true;
}

Process::Priority Process::GetPriority(PortProvider* port_provider) const {
  CHECK(IsValid());
  CHECK(port_provider);

  mach_port_t task_port = port_provider->TaskForHandle(Handle());
  if (task_port == TASK_NULL) {
    // Upon failure, return the default value.
    return Priority::kUserBlocking;
  }

  std::optional<task_role_t> task_role = GetTaskCategoryPolicyRole(task_port);
  if (!task_role) {
    // Upon failure, return the default value.
    return Priority::kUserBlocking;
  }
  bool is_suppression_policy_active = IsTaskSuppressionPolicyActive(task_port);
  if (*task_role == TASK_BACKGROUND_APPLICATION &&
      is_suppression_policy_active) {
    return Priority::kBestEffort;
  } else if (*task_role == TASK_BACKGROUND_APPLICATION &&
             !is_suppression_policy_active) {
    return Priority::kUserVisible;
  } else if (*task_role == TASK_FOREGROUND_APPLICATION &&
             !is_suppression_policy_active) {
    return Priority::kUserBlocking;
  }

  // It is possible to get a different state very early in the process lifetime,
  // before SetCurrentTaskDefaultRole() has been invoked. Assume highest
  // priority then.
  return Priority::kUserBlocking;
}

bool Process::SetPriority(PortProvider* port_provider, Priority priority) {
  CHECK(IsValid());
  CHECK(port_provider);

  if (!CanSetPriority()) {
    return false;
  }

  mach_port_t task_port = port_provider->TaskForHandle(Handle());
  if (task_port == TASK_NULL) {
    return false;
  }

  switch (priority) {
    case Priority::kBestEffort:
      // Activate the suppression policy.
      // Note:
      // App Nap keeps the task role to TASK_FOREGROUND_APPLICATION when it
      // activates the suppression policy. Here TASK_BACKGROUND_APPLICATION is
      // used instead to keep the kBestEffort role consistent with the value for
      // kUserVisible (so that its is not greater than kUserVisible). This
      // difference is unlikely to matter.
      return SetPriorityImpl(task_port, TASK_BACKGROUND_APPLICATION, true);
    case Priority::kUserVisible:
      // Set a task role with a lower priority than kUserBlocking, but do not
      // activate the suppression policy.
      return SetPriorityImpl(task_port, TASK_BACKGROUND_APPLICATION, false);
    case Priority::kUserBlocking:
    default:
      // Set the highest priority with the suppression policy inactive.
      return SetPriorityImpl(task_port, TASK_FOREGROUND_APPLICATION, false);
  }
}

// static
void Process::SetCurrentTaskDefaultRole() {
  if (!base::FeatureList::IsEnabled(kMacSetDefaultTaskRole)) {
    return;
  }

  SetTaskCategoryPolicy(mach_task_self(), TASK_FOREGROUND_APPLICATION);

  // Set the QoS settings to tier 0, to match the default value given to App Nap
  // enabled applications.
  task_qos_policy task_qos_policy = {
      .task_latency_qos_tier = LATENCY_QOS_TIER_0,
      .task_throughput_qos_tier = THROUGHPUT_QOS_TIER_0,
  };
  task_policy_set(mach_task_self(), TASK_BASE_QOS_POLICY,
                  reinterpret_cast<task_policy_t>(&task_qos_policy),
                  TASK_QOS_POLICY_COUNT);
}

}  // namespace base
