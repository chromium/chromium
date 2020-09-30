// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <errno.h>
#include <sched.h>
#include <stddef.h>
#include <cstdint>
#include <atomic>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/process/internal_linux.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread_internal_posix.h"
#include "base/threading/thread_id_name_manager.h"
#include "build/build_config.h"

#if !defined(OS_NACL) && !defined(OS_AIX)
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace base {

#if defined(OS_CHROMEOS)
const Feature kSchedUtilHints{"SchedUtilHints", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

namespace {

#if defined(OS_CHROMEOS)
std::atomic<bool> g_use_sched_util(true);
std::atomic<bool> g_feature_checked(false);

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

int sched_getattr(pid_t pid,
                  const struct sched_attr* attr,
                  unsigned int size,
                  unsigned int flags) {
  return syscall(__NR_sched_getattr, pid, attr, size, flags);
}

int sched_setattr(pid_t pid,
                  const struct sched_attr* attr,
                  unsigned int flags) {
  return syscall(__NR_sched_setattr, pid, attr, flags);
}
#endif  // OS_CHROMEOS

#if !defined(OS_NACL)
const FilePath::CharType kCgroupDirectory[] =
    FILE_PATH_LITERAL("/sys/fs/cgroup");

FilePath ThreadPriorityToCgroupDirectory(const FilePath& cgroup_filepath,
                                         ThreadPriority priority) {
  switch (priority) {
    case ThreadPriority::NORMAL:
      return cgroup_filepath;
    case ThreadPriority::BACKGROUND:
      return cgroup_filepath.Append(FILE_PATH_LITERAL("non-urgent"));
    case ThreadPriority::DISPLAY:
      FALLTHROUGH;
    case ThreadPriority::REALTIME_AUDIO:
      return cgroup_filepath.Append(FILE_PATH_LITERAL("urgent"));
  }
  NOTREACHED();
  return FilePath();
}

void SetThreadCgroup(PlatformThreadId thread_id,
                     const FilePath& cgroup_directory) {
  FilePath tasks_filepath = cgroup_directory.Append(FILE_PATH_LITERAL("tasks"));
  std::string tid = NumberToString(thread_id);
  int bytes_written = WriteFile(tasks_filepath, tid.c_str(), tid.size());
  if (bytes_written != static_cast<int>(tid.size())) {
    DVLOG(1) << "Failed to add " << tid << " to " << tasks_filepath.value();
  }
}

void SetThreadCgroupForThreadPriority(PlatformThreadId thread_id,
                                      const FilePath& cgroup_filepath,
                                      ThreadPriority priority) {
  // Append "chrome" suffix.
  FilePath cgroup_directory = ThreadPriorityToCgroupDirectory(
      cgroup_filepath.Append(FILE_PATH_LITERAL("chrome")), priority);

  // Silently ignore request if cgroup directory doesn't exist.
  if (!DirectoryExists(cgroup_directory))
    return;

  SetThreadCgroup(thread_id, cgroup_directory);
}

#if defined(OS_CHROMEOS)
// thread_id should always be the value in the root PID namespace (see
// FindThreadID).
void SetThreadLatencySensitivity(ProcessId process_id,
                                 PlatformThreadId thread_id,
                                 ThreadPriority priority) {
  struct sched_attr attr;
  bool is_urgent = false;
  int uclamp_min_urgent, uclamp_max_non_urgent, latency_sensitive_urgent;

  // Scheduler boost defaults to true unless disabled.
  if (!g_use_sched_util.load())
    return;

  // FieldTrial API can be called only once features were parsed.
  if (g_feature_checked.load()) {
    uclamp_min_urgent =
      GetFieldTrialParamByFeatureAsInt(kSchedUtilHints, "MinUrgent", 20);
    uclamp_max_non_urgent = GetFieldTrialParamByFeatureAsInt(
        kSchedUtilHints, "MaxNonUrgent", 100);
    latency_sensitive_urgent = GetFieldTrialParamByFeatureAsBool(
        kSchedUtilHints, "LatencySensitive", true);
  } else {
    // Use defaults if features were not parsed yet...
    uclamp_min_urgent = 20;
    uclamp_max_non_urgent = 100;
    latency_sensitive_urgent = true;
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

  switch (priority) {
    case ThreadPriority::NORMAL:
      FALLTHROUGH;
    case ThreadPriority::BACKGROUND:
      break;
    case ThreadPriority::DISPLAY:
      // Display needs a boost for consistent 60 fps compositing.
      FALLTHROUGH;
    case ThreadPriority::REALTIME_AUDIO:
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

  if (is_urgent) {
    attr.sched_util_min = uclamp_min_urgent;
    attr.sched_util_max = 100;
  } else {
    attr.sched_util_min = 0;
    attr.sched_util_max = uclamp_max_non_urgent;
  }

  attr.size = sizeof(struct sched_attr);
  if (sched_setattr(thread_id, &attr, 0) == -1) {
    // We log it as an error because, if the PathExists above succeeded, we
    // expect this syscall to also work since the kernel is new'ish.
    PLOG_IF(ERROR, errno != E2BIG)
        << "Failed to set sched_util_min, performance may be effected.\n";
  }
}
#endif

void SetThreadCgroupsForThreadPriority(PlatformThreadId thread_id,
                                       ThreadPriority priority) {
  FilePath cgroup_filepath(kCgroupDirectory);
  SetThreadCgroupForThreadPriority(
      thread_id, cgroup_filepath.Append(FILE_PATH_LITERAL("cpuset")), priority);
  SetThreadCgroupForThreadPriority(
      thread_id, cgroup_filepath.Append(FILE_PATH_LITERAL("schedtune")),
      priority);
}
#endif
}  // namespace

namespace internal {

namespace {
#if !defined(OS_NACL)
const struct sched_param kRealTimePrio = {8};
#endif
}  // namespace

const ThreadPriorityToNiceValuePair kThreadPriorityToNiceValueMap[4] = {
    {ThreadPriority::BACKGROUND, 10},
    {ThreadPriority::NORMAL, 0},
    {ThreadPriority::DISPLAY, -8},
    {ThreadPriority::REALTIME_AUDIO, -10},
};

Optional<bool> CanIncreaseCurrentThreadPriorityForPlatform(
    ThreadPriority priority) {
#if !defined(OS_NACL)
  // A non-zero soft-limit on RLIMIT_RTPRIO is required to be allowed to invoke
  // pthread_setschedparam in SetCurrentThreadPriorityForPlatform().
  struct rlimit rlim;
  if (priority == ThreadPriority::REALTIME_AUDIO &&
      getrlimit(RLIMIT_RTPRIO, &rlim) != 0 && rlim.rlim_cur != 0) {
    return base::make_optional(true);
  }
#endif
  return base::nullopt;
}

bool SetCurrentThreadPriorityForPlatform(ThreadPriority priority) {
#if !defined(OS_NACL)
  // For legacy schedtune interface
  SetThreadCgroupsForThreadPriority(PlatformThread::CurrentId(), priority);

#if defined(OS_CHROMEOS)
  // For upstream uclamp interface. We try both legacy (schedtune, as done
  // earlier) and upstream (uclamp) interfaces, and whichever succeeds wins.
  SetThreadLatencySensitivity(0 /* ignore */, 0 /* thread-self */, priority);
#endif

  return priority == ThreadPriority::REALTIME_AUDIO &&
         pthread_setschedparam(pthread_self(), SCHED_RR, &kRealTimePrio) == 0;
#else
  return false;
#endif
}

Optional<ThreadPriority> GetCurrentThreadPriorityForPlatform() {
#if !defined(OS_NACL)
  int maybe_sched_rr = 0;
  struct sched_param maybe_realtime_prio = {0};
  if (pthread_getschedparam(pthread_self(), &maybe_sched_rr,
                            &maybe_realtime_prio) == 0 &&
      maybe_sched_rr == SCHED_RR &&
      maybe_realtime_prio.sched_priority == kRealTimePrio.sched_priority) {
    return base::make_optional(ThreadPriority::REALTIME_AUDIO);
  }
#endif
  return base::nullopt;
}

}  // namespace internal

// static
void PlatformThread::SetName(const std::string& name) {
  ThreadIdNameManager::GetInstance()->SetName(name);

#if !defined(OS_NACL) && !defined(OS_AIX)
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
#endif  //  !defined(OS_NACL) && !defined(OS_AIX)
}

#if !defined(OS_NACL) && !defined(OS_AIX)
// static
void PlatformThread::SetThreadPriority(ProcessId process_id,
                                       PlatformThreadId thread_id,
                                       ThreadPriority priority) {
  // Changing current main threads' priority is not permitted in favor of
  // security, this interface is restricted to change only non-main thread
  // priority.
  CHECK_NE(thread_id, process_id);

  // For legacy schedtune interface
  SetThreadCgroupsForThreadPriority(thread_id, priority);

#if defined(OS_CHROMEOS)
  // For upstream uclamp interface. We try both legacy (schedtune, as done
  // earlier) and upstream (uclamp) interfaces, and whichever succeeds wins.
  SetThreadLatencySensitivity(process_id, thread_id, priority);
#endif

  const int nice_setting = internal::ThreadPriorityToNiceValue(priority);
  if (setpriority(PRIO_PROCESS, thread_id, nice_setting)) {
    DVPLOG(1) << "Failed to set nice value of thread (" << thread_id << ") to "
              << nice_setting;
  }
}
#endif  //  !defined(OS_NACL) && !defined(OS_AIX)

#if defined(OS_CHROMEOS)
void PlatformThread::InitThreadPostFieldTrial() {
  DCHECK(FeatureList::GetInstance());
  if (!FeatureList::IsEnabled(kSchedUtilHints)) {
    g_use_sched_util.store(false);
  }
  g_feature_checked.store(true);
}
#endif

void InitThreading() {}

void TerminateOnThread() {}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
#if !defined(THREAD_SANITIZER)
  return 0;
#else
  // ThreadSanitizer bloats the stack heavily. Evidence has been that the
  // default stack size isn't enough for some browser tests.
  return 2 * (1 << 23);  // 2 times 8192K (the default stack size on Linux).
#endif
}

}  // namespace base
