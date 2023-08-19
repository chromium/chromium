// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Description: ChromeOS specific Linux code layered on top of
// base/threading/platform_thread_linux{,_base}.cc.

#include "base/threading/platform_thread.h"
#include "base/threading/platform_thread_internal_posix.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/internal_linux.h"
#include "base/strings/stringprintf.h"

#include <sys/resource.h>

namespace base {

BASE_FEATURE(kSchedUtilHints,
             "SchedUtilHints",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

std::atomic<bool> g_use_sched_util(true);
std::atomic<bool> g_scheduler_hints_adjusted(false);

// When a device doesn't specify uclamp values via chrome switches,
// default boosting for urgent tasks is hardcoded here as 20%.
// Higher values can lead to higher power consumption thus this value
// is chosen conservatively where it does not show noticeable
// power usage increased from several perf/power tests.
const int kSchedulerBoostDef = 20;
const int kSchedulerLimitDef = 100;
const bool kSchedulerUseLatencyTuneDef = true;

int g_scheduler_boost_adj;
int g_scheduler_limit_adj;
bool g_scheduler_use_latency_tune_adj;

// Defined by linux uclamp ABI of sched_setattr().
constexpr uint32_t kSchedulerUclampMin = 0;
constexpr uint32_t kSchedulerUclampMax = 1024;

// sched_attr is used to set scheduler attributes for Linux. It is not a POSIX
// struct and glibc does not expose it.
struct sched_attr {
  uint32_t size;

  uint32_t sched_policy;
  uint64_t sched_flags;

  /* SCHED_NORMAL, SCHED_BATCH */
  int32_t sched_nice;

  /* SCHED_FIFO, SCHED_RR */
  uint32_t sched_priority;

  /* SCHED_DEADLINE */
  uint64_t sched_runtime;
  uint64_t sched_deadline;
  uint64_t sched_period;

  /* Utilization hints */
  uint32_t sched_util_min;
  uint32_t sched_util_max;
};

#if !defined(__NR_sched_setattr)
#if defined(__x86_64__)
#define __NR_sched_setattr 314
#define __NR_sched_getattr 315
#elif defined(__i386__)
#define __NR_sched_setattr 351
#define __NR_sched_getattr 352
#elif defined(__arm__)
#define __NR_sched_setattr 380
#define __NR_sched_getattr 381
#elif defined(__aarch64__)
#define __NR_sched_setattr 274
#define __NR_sched_getattr 275
#else
#error "We don't have an __NR_sched_setattr for this architecture."
#endif
#endif

#if !defined(SCHED_FLAG_UTIL_CLAMP_MIN)
#define SCHED_FLAG_UTIL_CLAMP_MIN 0x20
#endif

#if !defined(SCHED_FLAG_UTIL_CLAMP_MAX)
#define SCHED_FLAG_UTIL_CLAMP_MAX 0x40
#endif

long sched_getattr(pid_t pid,
                   const struct sched_attr* attr,
                   unsigned int size,
                   unsigned int flags) {
  return syscall(__NR_sched_getattr, pid, attr, size, flags);
}

long sched_setattr(pid_t pid,
                   const struct sched_attr* attr,
                   unsigned int flags) {
  return syscall(__NR_sched_setattr, pid, attr, flags);
}

// Setup whether a thread is latency sensitive. The thread_id should
// always be the value in the root PID namespace (see FindThreadID).
void SetThreadLatencySensitivity(ProcessId process_id,
                                 PlatformThreadId thread_id,
                                 ThreadType thread_type) {
  struct sched_attr attr;
  bool is_urgent = false;
  int boost_percent, limit_percent;
  int latency_sensitive_urgent;

  // Scheduler boost defaults to true unless disabled.
  if (!g_use_sched_util.load())
    return;

  // FieldTrial API can be called only once features were parsed.
  if (g_scheduler_hints_adjusted.load()) {
    boost_percent = g_scheduler_boost_adj;
    limit_percent = g_scheduler_limit_adj;
    latency_sensitive_urgent = g_scheduler_use_latency_tune_adj;
  } else {
    boost_percent = kSchedulerBoostDef;
    limit_percent = kSchedulerLimitDef;
    latency_sensitive_urgent = kSchedulerUseLatencyTuneDef;
  }

  // The thread_id passed in here is either 0 (in which case we ste for current
  // thread), or is a tid that is not the NS tid but the global one. The
  // conversion from NS tid to global tid is done by the callers using
  // FindThreadID().
  FilePath thread_dir;
  if (thread_id && thread_id != PlatformThread::CurrentId())
    thread_dir = FilePath(StringPrintf("/proc/%d/task/%d/", process_id, thread_id));
  else
    thread_dir = FilePath("/proc/thread-self/");

  FilePath latency_sensitive_file = thread_dir.Append("latency_sensitive");

  if (!PathExists(latency_sensitive_file))
    return;

  // Silently ignore if getattr fails due to sandboxing.
  if (sched_getattr(thread_id, &attr, sizeof(attr), 0) == -1 ||
      attr.size != sizeof(attr))
    return;

  switch (thread_type) {
    case ThreadType::kBackground:
    case ThreadType::kUtility:
    case ThreadType::kResourceEfficient:
    case ThreadType::kDefault:
      break;
    case ThreadType::kCompositing:
    case ThreadType::kDisplayCritical:
      // Compositing and display critical threads need a boost for consistent 60
      // fps.
      [[fallthrough]];
    case ThreadType::kRealtimeAudio:
      is_urgent = true;
      break;
  }

  PLOG_IF(ERROR,
          !WriteFile(latency_sensitive_file,
                     (is_urgent && latency_sensitive_urgent) ? "1" : "0", 1))
      << "Failed to write latency file.";

  attr.sched_flags |= SCHED_FLAG_UTIL_CLAMP_MIN;
  attr.sched_flags |= SCHED_FLAG_UTIL_CLAMP_MAX;

  if (is_urgent) {
    attr.sched_util_min =
        (saturated_cast<uint32_t>(boost_percent) * kSchedulerUclampMax + 50) /
        100;
    attr.sched_util_max = kSchedulerUclampMax;
  } else {
    attr.sched_util_min = kSchedulerUclampMin;
    attr.sched_util_max =
        (saturated_cast<uint32_t>(limit_percent) * kSchedulerUclampMax + 50) /
        100;
  }

  DCHECK_GE(attr.sched_util_min, kSchedulerUclampMin);
  DCHECK_LE(attr.sched_util_max, kSchedulerUclampMax);

  attr.size = sizeof(struct sched_attr);
  if (sched_setattr(thread_id, &attr, 0) == -1) {
    // We log it as an error because, if the PathExists above succeeded, we
    // expect this syscall to also work since the kernel is new'ish.
    PLOG_IF(ERROR, errno != E2BIG)
        << "Failed to set sched_util_min, performance may be effected.";
  }
}

} // namespace

void PlatformThreadChromeOS::InitFeaturesPostFieldTrial() {
  DCHECK(FeatureList::GetInstance());
  if (!FeatureList::IsEnabled(kSchedUtilHints)) {
    g_use_sched_util.store(false);
    return;
  }

  int boost_def = kSchedulerBoostDef;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSchedulerBoostUrgent)) {
    std::string boost_switch_str =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kSchedulerBoostUrgent);

    int boost_switch_val;
    if (!StringToInt(boost_switch_str, &boost_switch_val) ||
        boost_switch_val < 0 || boost_switch_val > 100) {
      DVLOG(1) << "Invalid input for " << switches::kSchedulerBoostUrgent;
    } else {
      boost_def = boost_switch_val;
    }
  }

  g_scheduler_boost_adj = GetFieldTrialParamByFeatureAsInt(
      kSchedUtilHints, "BoostUrgent", boost_def);
  g_scheduler_limit_adj = GetFieldTrialParamByFeatureAsInt(
      kSchedUtilHints, "LimitNonUrgent", kSchedulerLimitDef);
  g_scheduler_use_latency_tune_adj = GetFieldTrialParamByFeatureAsBool(
      kSchedUtilHints, "LatencyTune", kSchedulerUseLatencyTuneDef);

  g_scheduler_hints_adjusted.store(true);
}

// static
void PlatformThreadChromeOS::SetThreadType(ProcessId process_id,
                                           PlatformThreadId thread_id,
                                           ThreadType thread_type) {
  // TODO(b/262267726): Call PlatformThreadLinux::SetThreadType for common code.
  PlatformThreadId syscall_tid = thread_id;
  if (thread_id == PlatformThread::CurrentId()) {
    syscall_tid = 0;
  }

  // For legacy schedtune interface
  PlatformThreadLinux::SetThreadCgroupsForThreadType(thread_id, thread_type);

  // For upstream uclamp interface. We try both legacy (schedtune, as done
  // earlier) and upstream (uclamp) interfaces, and whichever succeeds wins.
  SetThreadLatencySensitivity(process_id, thread_id, thread_type);

  if (thread_type == ThreadType::kRealtimeAudio) {
    if (sched_setscheduler(syscall_tid, SCHED_RR, &kRealTimePrio) == 0) {
      return;
    }
    // If failed to set to RT, fallback to setting nice value.
    DVPLOG(1) << "Failed to set realtime priority for thread (" << thread_id
              << ")";
  }

  const int nice_setting = internal::ThreadTypeToNiceValue(thread_type);
  if (setpriority(PRIO_PROCESS, static_cast<id_t>(syscall_tid), nice_setting)) {
    DVPLOG(1) << "Failed to set nice value of thread (" << thread_id << ") to "
              << nice_setting;
  }
}

}  // namespace base
