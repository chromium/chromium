// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Description: Linux specific functionality. Other Linux-derivatives layer on
// top of this translation unit.

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

#include <pthread.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

namespace base {

namespace {

ThreadTypeDelegate* g_thread_type_delegate = nullptr;

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

}  // namespace

namespace internal {

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
  // A non-zero soft-limit on RLIMIT_RTPRIO is required to be allowed to invoke
  // pthread_setschedparam in SetCurrentThreadTypeForPlatform().
  struct rlimit rlim;
  return getrlimit(RLIMIT_RTPRIO, &rlim) != 0 && rlim.rlim_cur != 0;
}

bool SetCurrentThreadTypeForPlatform(ThreadType thread_type,
                                     MessagePumpType pump_type_hint) {
  const PlatformThreadId tid = PlatformThread::CurrentId();

  if (g_thread_type_delegate &&
      g_thread_type_delegate->HandleThreadTypeChange(tid, thread_type)) {
    return true;
  }

  PlatformThread::SetThreadType(getpid(), tid, thread_type);
  return true;
}

absl::optional<ThreadPriorityForTest>
GetCurrentThreadPriorityForPlatformForTest() {
  int maybe_sched_rr = 0;
  struct sched_param maybe_realtime_prio = {0};
  if (pthread_getschedparam(pthread_self(), &maybe_sched_rr,
                            &maybe_realtime_prio) == 0 &&
      maybe_sched_rr == SCHED_RR &&
      maybe_realtime_prio.sched_priority ==
          PlatformThreadLinux::kRealTimePrio.sched_priority) {
    return absl::make_optional(ThreadPriorityForTest::kRealtimeAudio);
  }
  return absl::nullopt;
}

}  // namespace internal

void PlatformThreadBase::SetName(const std::string& name) {
  SetNameCommon(name);

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
}

// static
void PlatformThreadLinux::SetThreadTypeDelegate(ThreadTypeDelegate* delegate) {
  // A component cannot override a delegate set by another component, thus
  // disallow setting a delegate when one already exists.
  DCHECK(!g_thread_type_delegate || !delegate);

  g_thread_type_delegate = delegate;
}

// static
void PlatformThreadLinux::SetThreadCgroupsForThreadType(
    PlatformThreadId thread_id,
    ThreadType thread_type) {
  FilePath cgroup_filepath(kCgroupDirectory);
  SetThreadCgroupForThreadType(
      thread_id, cgroup_filepath.Append(FILE_PATH_LITERAL("cpuset")),
      thread_type);
  SetThreadCgroupForThreadType(
      thread_id, cgroup_filepath.Append(FILE_PATH_LITERAL("schedtune")),
      thread_type);
}

// static
void PlatformThreadLinux::SetThreadType(ProcessId process_id,
                                        PlatformThreadId thread_id,
                                        ThreadType thread_type) {
  SetThreadCgroupsForThreadType(thread_id, thread_type);

  // Some scheduler syscalls require thread ID of 0 for current thread.
  // This prevents us from requiring to translate the NS TID to
  // global TID.
  PlatformThreadId syscall_tid = thread_id;
  if (thread_id == PlatformThread::CurrentId()) {
    syscall_tid = 0;
  }

  if (thread_type == ThreadType::kRealtimeAudio) {
    if (sched_setscheduler(syscall_tid, SCHED_RR,
                           &PlatformThreadLinux::kRealTimePrio) == 0) {
      return;
    }
    // If failed to set to RT, fallback to setpriority to set nice value.
    DPLOG(ERROR) << "Failed to set realtime priority for thread " << thread_id;
  }

  const int nice_setting = internal::ThreadTypeToNiceValue(thread_type);
  if (setpriority(PRIO_PROCESS, static_cast<id_t>(syscall_tid), nice_setting)) {
    DVPLOG(1) << "Failed to set nice value of thread (" << thread_id << ") to "
              << nice_setting;
  }
}

}  // namespace base
