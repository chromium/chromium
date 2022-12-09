// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <errno.h>
#include <sched.h>
#include <stddef.h>
#include <cstdint>
#include <atomic>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/process/internal_linux.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread_internal_posix.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_type_delegate.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_AIX)
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace base {

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kSchedUtilHints,
             "SchedUtilHints",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

namespace {

#if !BUILDFLAG(IS_NACL)
ThreadTypeDelegate* g_thread_type_delegate = nullptr;
#endif

#if BUILDFLAG(IS_CHROMEOS)
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

#if !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_AIX)

// Defined by linux uclamp ABI of sched_setattr().
const uint32_t kSchedulerUclampMin = 0;
const uint32_t kSchedulerUclampMax = 1024;

// sched_attr is used to set scheduler attributes for Linux. It is not a POSIX
// struct and glibc does not expose it.
struct sched_attr {
  uint32_t size;

  uint32_t sched_policy;
  uint64_t sched_flags;

  /* SCHED_NORMAL, SCHED_BATCH */
  __s32 sched_nice;

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
#endif  // !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_AIX)
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_NACL)
const FilePath::CharType kCgroupDirectory[] =
    FILE_PATH_LITERAL("/sys/fs/cgroup");

FilePath ThreadTypeToCgroupDirectory(const FilePath& cgroup_filepath,
                                     ThreadType thread_type) {
  switch (thread_type) {
    case ThreadType::kBackground:
    case ThreadType::kUtility:
    case ThreadType::kResourceEfficient:
      return cgroup_filepath.Append(FILE_PATH_LITERAL("non-urgent"));
    case ThreadType::kDefault:
      return cgroup_filepath;
    case ThreadType::kCompositing:
#if BUILDFLAG(IS_CHROMEOS)
      // On ChromeOS, kCompositing is also considered urgent.
      return cgroup_filepath.Append(FILE_PATH_LITERAL("urgent"));
#else
      // TODO(1329208): Experiment with bringing IS_LINUX inline with
      // IS_CHROMEOS.
      return cgroup_filepath;
#endif
    case ThreadType::kDisplayCritical:
    case ThreadType::kRealtimeAudio:
      return cgroup_filepath.Append(FILE_PATH_LITERAL("urgent"));
  }
  NOTREACHED();
  return FilePath();
}

void SetThreadCgroup(PlatformThreadId thread_id,
                     const FilePath& cgroup_directory) {
  FilePath tasks_filepath = cgroup_directory.Append(FILE_PATH_LITERAL("tasks"));
  std::string tid = NumberToString(thread_id);
  // TODO(crbug.com/1333521): Remove cast.
  const int size = static_cast<int>(tid.size());
  int bytes_written = WriteFile(tasks_filepath, tid.data(), size);
  if (bytes_written != size) {
    DVLOG(1) << "Failed to add " << tid << " to " << tasks_filepath.value();
  }
}

void SetThreadCgroupForThreadType(PlatformThreadId thread_id,
                                  const FilePath& cgroup_filepath,
                                  ThreadType thread_type) {
  // Append "chrome" suffix.
  FilePath cgroup_directory = ThreadTypeToCgroupDirectory(
      cgroup_filepath.Append(FILE_PATH_LITERAL("chrome")), thread_type);

  // Silently ignore request if cgroup directory doesn't exist.
  if (!DirectoryExists(cgroup_directory))
    return;

  SetThreadCgroup(thread_id, cgroup_directory);
}

#if BUILDFLAG(IS_CHROMEOS)
// thread_id should always be the value in the root PID namespace (see
// FindThreadID).
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
  std::string thread_dir;
  if (thread_id)
    thread_dir = base::StringPrintf("/proc/%d/task/%d/", process_id, thread_id);
  else
    thread_dir = "/proc/thread-self/";

  // Silently ignore request if thread directory doesn't exist.
  if (!DirectoryExists(FilePath(thread_dir)))
    return;

  FilePath latency_sensitive_file = FilePath(thread_dir + "latency_sensitive");

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

  if (is_urgent && latency_sensitive_urgent) {
    PLOG_IF(ERROR, !WriteFile(latency_sensitive_file, "1", 1))
        << "Failed to write latency file.\n";
  } else {
    PLOG_IF(ERROR, !WriteFile(latency_sensitive_file, "0", 1))
        << "Failed to write latency file.\n";
  }

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
        << "Failed to set sched_util_min, performance may be effected.\n";
  }
}
#endif

void SetThreadCgroupsForThreadType(PlatformThreadId thread_id,
                                   ThreadType thread_type) {
  FilePath cgroup_filepath(kCgroupDirectory);
  SetThreadCgroupForThreadType(
      thread_id, cgroup_filepath.Append(FILE_PATH_LITERAL("cpuset")),
      thread_type);
  SetThreadCgroupForThreadType(
      thread_id, cgroup_filepath.Append(FILE_PATH_LITERAL("schedtune")),
      thread_type);
}
#endif
}  // namespace

namespace internal {

namespace {
#if !BUILDFLAG(IS_NACL)
const struct sched_param kRealTimePrio = {8};
#endif
}  // namespace

const ThreadPriorityToNiceValuePairForTest
    kThreadPriorityToNiceValueMapForTest[5] = {
        {ThreadPriorityForTest::kRealtimeAudio, -10},
        {ThreadPriorityForTest::kDisplay, -8},
        {ThreadPriorityForTest::kNormal, 0},
        {ThreadPriorityForTest::kUtility, 1},
        {ThreadPriorityForTest::kBackground, 10},
};

const ThreadTypeToNiceValuePair kThreadTypeToNiceValueMap[7] = {
    {ThreadType::kBackground, 10},       {ThreadType::kUtility, 1},
    {ThreadType::kResourceEfficient, 0}, {ThreadType::kDefault, 0},
#if BUILDFLAG(IS_CHROMEOS)
    {ThreadType::kCompositing, -8},
#else
    // TODO(1329208): Experiment with bringing IS_LINUX inline with IS_CHROMEOS.
    {ThreadType::kCompositing, 0},
#endif
    {ThreadType::kDisplayCritical, -8},  {ThreadType::kRealtimeAudio, -10},
};

bool CanSetThreadTypeToRealtimeAudio() {
#if !BUILDFLAG(IS_NACL)
  // A non-zero soft-limit on RLIMIT_RTPRIO is required to be allowed to invoke
  // pthread_setschedparam in SetCurrentThreadTypeForPlatform().
  struct rlimit rlim;
  return getrlimit(RLIMIT_RTPRIO, &rlim) != 0 && rlim.rlim_cur != 0;
#else
  return false;
#endif
}

bool SetCurrentThreadTypeForPlatform(ThreadType thread_type,
                                     MessagePumpType pump_type_hint) {
#if !BUILDFLAG(IS_NACL)
  const PlatformThreadId tid = PlatformThread::CurrentId();

  if (g_thread_type_delegate &&
      g_thread_type_delegate->HandleThreadTypeChange(tid, thread_type)) {
    return true;
  }

  // For legacy schedtune interface
  SetThreadCgroupsForThreadType(tid, thread_type);

#if BUILDFLAG(IS_CHROMEOS)
  // For upstream uclamp interface. We try both legacy (schedtune, as done
  // earlier) and upstream (uclamp) interfaces, and whichever succeeds wins.
  SetThreadLatencySensitivity(0 /* ignore */, 0 /* thread-self */, thread_type);
#endif

  return thread_type == ThreadType::kRealtimeAudio &&
         pthread_setschedparam(pthread_self(), SCHED_RR, &kRealTimePrio) == 0;
#else
  return false;
#endif
}

absl::optional<ThreadPriorityForTest>
GetCurrentThreadPriorityForPlatformForTest() {
#if !BUILDFLAG(IS_NACL)
  int maybe_sched_rr = 0;
  struct sched_param maybe_realtime_prio = {0};
  if (pthread_getschedparam(pthread_self(), &maybe_sched_rr,
                            &maybe_realtime_prio) == 0 &&
      maybe_sched_rr == SCHED_RR &&
      maybe_realtime_prio.sched_priority == kRealTimePrio.sched_priority) {
    return absl::make_optional(ThreadPriorityForTest::kRealtimeAudio);
  }
#endif
  return absl::nullopt;
}

}  // namespace internal

// static
void PlatformThread::SetName(const std::string& name) {
  ThreadIdNameManager::GetInstance()->SetName(name);

#if !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_AIX)
  // On linux we can get the thread names to show up in the debugger by setting
  // the process name for the LWP.  We don't want to do this for the main
  // thread because that would rename the process, causing tools like killall
  // to stop working.
  if (PlatformThread::CurrentId() == getpid())
    return;

  // http://0pointer.de/blog/projects/name-your-threads.html
  // Set the name for the LWP (which gets truncated to 15 characters).
  // Note that glibc also has a 'pthread_setname_np' api, but it may not be
  // available everywhere and it's only benefit over using prctl directly is
  // that it can set the name of threads other than the current thread.
  int err = prctl(PR_SET_NAME, name.c_str());
  // We expect EPERM failures in sandboxed processes, just ignore those.
  if (err < 0 && errno != EPERM)
    DPLOG(ERROR) << "prctl(PR_SET_NAME)";
#endif  //  !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_AIX)
}

#if !BUILDFLAG(IS_NACL)
// static
void PlatformThread::SetThreadTypeDelegate(ThreadTypeDelegate* delegate) {
  // A component cannot override a delegate set by another component, thus
  // disallow setting a delegate when one already exists.
  DCHECK(!g_thread_type_delegate || !delegate);

  g_thread_type_delegate = delegate;
}
#endif

#if !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_AIX)
// static
void PlatformThread::SetThreadType(ProcessId process_id,
                                   PlatformThreadId thread_id,
                                   ThreadType thread_type) {
  // Changing current main threads' priority is not permitted in favor of
  // security, this interface is restricted to change only non-main thread
  // priority.
  CHECK_NE(thread_id, process_id);

  // For legacy schedtune interface
  SetThreadCgroupsForThreadType(thread_id, thread_type);

#if BUILDFLAG(IS_CHROMEOS)
  // For upstream uclamp interface. We try both legacy (schedtune, as done
  // earlier) and upstream (uclamp) interfaces, and whichever succeeds wins.
  SetThreadLatencySensitivity(process_id, thread_id, thread_type);
#endif

  const int nice_setting = internal::ThreadTypeToNiceValue(thread_type);
  if (setpriority(PRIO_PROCESS, static_cast<id_t>(thread_id), nice_setting)) {
    DVPLOG(1) << "Failed to set nice value of thread (" << thread_id << ") to "
              << nice_setting;
  }
}
#endif  //  !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_AIX)

#if BUILDFLAG(IS_CHROMEOS)
void PlatformThread::InitFeaturesPostFieldTrial() {
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
      DVPLOG(1) << "Invalid input for " << switches::kSchedulerBoostUrgent;
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
#endif

void InitThreading() {}

void TerminateOnThread() {}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
#if !defined(THREAD_SANITIZER) && defined(__GLIBC__)
  // Generally glibc sets ample default stack sizes, so use the default there.
  return 0;
#elif !defined(THREAD_SANITIZER)
  // Other libcs (uclibc, musl, etc) tend to use smaller stacks, often too small
  // for chromium. Make sure we have enough space to work with here. Note that
  // for comparison glibc stacks are generally around 8MB.
  return 2 * (1 << 20);
#else
  // ThreadSanitizer bloats the stack heavily. Evidence has been that the
  // default stack size isn't enough for some browser tests.
  return 2 * (1 << 23);  // 2 times 8192K (the default stack size on Linux).
#endif
}

}  // namespace base
