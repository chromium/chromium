// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Description: ChromeOS specific Linux code layered on top of
// base/threading/platform_thread_linux{,_base}.cc.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/process/internal_linux.h"
#include "base/process/process.h"
#include "base/strings/stringprintf.h"
#include "base/threading/cross_process_platform_thread_delegate.h"
#include "base/threading/platform_thread.h"
#include "base/threading/platform_thread_internal_posix.h"

#include <sys/resource.h>

namespace base {

BASE_FEATURE(kSchedUtilHints,
             "SchedUtilHints",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSetThreadBgForBgProcess,
             "SetThreadBgForBgProcess",
             FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSetRtForDisplayThreads,
             "SetRtForDisplayThreads",
             FEATURE_DISABLED_BY_DEFAULT);
namespace {

CrossProcessPlatformThreadDelegate* g_cross_process_platform_thread_delegate =
    nullptr;

std::atomic<bool> g_use_sched_util(true);
std::atomic<bool> g_scheduler_hints_adjusted(false);
std::atomic<bool> g_threads_bg_enabled(false);
std::atomic<bool> g_display_threads_rt(false);

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
    case ThreadType::kDisplayCritical:
      // Compositing and display critical threads need a boost for consistent 60
      // fps.
      [[fallthrough]];
    case ThreadType::kRealtimeAudio:
      is_urgent = true;
      break;
  }

  PLOG_IF(ERROR, !WriteFile(latency_sensitive_file,
                            (is_urgent && latency_sensitive_urgent)
                                ? base::byte_span_from_cstring("1")
                                : base::byte_span_from_cstring("0")))
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

// Get the type by reading through kThreadTypeToNiceValueMap
std::optional<ThreadType> GetThreadTypeForNiceValue(int nice_value) {
  for (auto i : internal::kThreadTypeToNiceValueMap) {
    if (nice_value == i.nice_value) {
      return i.thread_type;
    }
  }
  return std::nullopt;
}

std::optional<int> GetNiceValueForThreadId(PlatformThreadId thread_id) {
  // Get the current nice value of the thread_id
  errno = 0;
  int nice_value = getpriority(PRIO_PROCESS, static_cast<id_t>(thread_id));
  if (nice_value == -1 && errno != 0) {
    // The thread may disappear for any reason so ignore ESRCH.
    DVPLOG_IF(1, errno != ESRCH)
        << "Failed to call getpriority for thread id " << thread_id
        << ", performance may be effected.";
    return std::nullopt;
  }
  return nice_value;
}

} // namespace

void SetThreadTypeOtherAttrs(ProcessId process_id,
                             PlatformThreadId thread_id,
                             ThreadType thread_type) {
  // For cpuset and legacy schedtune interface
  PlatformThreadLinux::SetThreadCgroupsForThreadType(thread_id, thread_type);

  // For upstream uclamp interface. We try both legacy (schedtune, as done
  // earlier) and upstream (uclamp) interfaces, and whichever succeeds wins.
  SetThreadLatencySensitivity(process_id, thread_id, thread_type);
}

// Set or reset the RT priority of a thread based on its type
// and whether the process it is in is backgrounded.
// Setting an RT task to CFS retains the task's nice value.
void SetThreadRTPrioFromType(ProcessId process_id,
                             PlatformThreadId thread_id,
                             ThreadType thread_type,
                             bool proc_bg) {
  struct sched_param prio;
  int policy;

  switch (thread_type) {
    case ThreadType::kRealtimeAudio:
      prio = PlatformThreadChromeOS::kRealTimeAudioPrio;
      policy = SCHED_RR;
      break;
    case ThreadType::kDisplayCritical:
      if (!PlatformThreadChromeOS::IsDisplayThreadsRtFeatureEnabled()) {
        return;
      }
      if (proc_bg) {
        // Per manpage, must be 0. Otherwise could have passed nice value here.
        // Note that even though the prio.sched_priority passed to the
        // sched_setscheduler() syscall is 0, the old nice value (which holds the
        // ThreadType of the thread) is retained.
        prio.sched_priority = 0;
        policy = SCHED_OTHER;
      } else {
        prio = PlatformThreadChromeOS::kRealTimeDisplayPrio;
        policy = SCHED_RR;
      }
      break;
    default:
      return;
  }

  PlatformThreadId syscall_tid = thread_id == PlatformThread::CurrentId() ? 0 : thread_id;
  if (sched_setscheduler(syscall_tid, policy, &prio) != 0) {
    DVPLOG(1) << "Failed to set policy/priority for thread " << thread_id;
  }
}

void SetThreadNiceFromType(ProcessId process_id,
                           PlatformThreadId thread_id,
                           ThreadType thread_type) {
  PlatformThreadId syscall_tid = thread_id == PlatformThread::CurrentId() ? 0 : thread_id;
  const int nice_setting = internal::ThreadTypeToNiceValue(thread_type);
  if (setpriority(PRIO_PROCESS, static_cast<id_t>(syscall_tid), nice_setting)) {
    DVPLOG(1) << "Failed to set nice value of thread " << thread_id << " to "
              << nice_setting;
  }
}

void PlatformThreadChromeOS::InitializeFeatures() {
  DCHECK(FeatureList::GetInstance());
  g_threads_bg_enabled.store(FeatureList::IsEnabled(kSetThreadBgForBgProcess));
  g_display_threads_rt.store(FeatureList::IsEnabled(kSetRtForDisplayThreads));
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
void PlatformThreadChromeOS::SetCrossProcessPlatformThreadDelegate(
    CrossProcessPlatformThreadDelegate* delegate) {
  // A component cannot override a delegate set by another component, thus
  // disallow setting a delegate when one already exists.
  DCHECK_NE(!!g_cross_process_platform_thread_delegate, !!delegate);

  g_cross_process_platform_thread_delegate = delegate;
}

// static
bool PlatformThreadChromeOS::IsThreadsBgFeatureEnabled() {
  return g_threads_bg_enabled.load();
}

// static
bool PlatformThreadChromeOS::IsDisplayThreadsRtFeatureEnabled() {
  return g_display_threads_rt.load();
}

// static
std::optional<ThreadType> PlatformThreadChromeOS::GetThreadTypeFromThreadId(
    ProcessId process_id,
    PlatformThreadId thread_id) {
  // Get the current nice_value of the thread_id
  std::optional<int> nice_value = GetNiceValueForThreadId(thread_id);
  if (!nice_value.has_value()) {
    return std::nullopt;
  }
  return GetThreadTypeForNiceValue(nice_value.value());
}

// static
void PlatformThreadChromeOS::SetThreadType(ProcessId process_id,
                                           PlatformThreadId thread_id,
                                           ThreadType thread_type,
                                           IsViaIPC via_ipc) {
  if (g_cross_process_platform_thread_delegate &&
      g_cross_process_platform_thread_delegate->HandleThreadTypeChange(
          process_id, thread_id, thread_type)) {
    return;
  }
  internal::SetThreadType(process_id, thread_id, thread_type, via_ipc);
}

void PlatformThreadChromeOS::SetThreadBackgrounded(ProcessId process_id,
                                                   PlatformThreadId thread_id,
                                                   bool backgrounded) {
  // Get the current nice value of the thread_id
  std::optional<int> nice_value = GetNiceValueForThreadId(thread_id);
  if (!nice_value.has_value()) {
    return;
  }

  std::optional<ThreadType> type =
      GetThreadTypeForNiceValue(nice_value.value());
  if (!type.has_value()) {
    return;
  }

  // kRealtimeAudio threads are not backgrounded or foregrounded.
  if (type == ThreadType::kRealtimeAudio) {
    return;
  }

  SetThreadTypeOtherAttrs(
      process_id, thread_id,
      backgrounded ? ThreadType::kBackground : type.value());
  SetThreadRTPrioFromType(process_id, thread_id, type.value(), backgrounded);
}

SequenceCheckerImpl&
PlatformThreadChromeOS::GetCrossProcessThreadPrioritySequenceChecker() {
  // In order to use a NoDestructor instance, use SequenceCheckerImpl instead of
  // SequenceCheckerDoNothing because SequenceCheckerImpl is trivially
  // destructible but SequenceCheckerDoNothing isn't.
  static NoDestructor<SequenceCheckerImpl> instance;
  return *instance;
}

namespace internal {

void SetThreadTypeChromeOS(ProcessId process_id,
                           PlatformThreadId thread_id,
                           ThreadType thread_type,
                           IsViaIPC via_ipc) {
  // TODO(b/262267726): Re-use common code with SetThreadTypeLinux.
  // Should not be called concurrently with
  // other functions like SetThreadBackgrounded.
  if (via_ipc) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        PlatformThread::GetCrossProcessThreadPrioritySequenceChecker());
  }

  auto proc = Process::Open(process_id);
  bool backgrounded = false;
  if (PlatformThread::IsThreadsBgFeatureEnabled() &&
      thread_type != ThreadType::kRealtimeAudio && proc.IsValid() &&
      proc.GetPriority() == base::Process::Priority::kBestEffort) {
    backgrounded = true;
  }

  SetThreadTypeOtherAttrs(process_id, thread_id,
                          backgrounded ? ThreadType::kBackground : thread_type);

  SetThreadRTPrioFromType(process_id, thread_id, thread_type, backgrounded);
  SetThreadNiceFromType(process_id, thread_id, thread_type);
}

}  // namespace internal

}  // namespace base
