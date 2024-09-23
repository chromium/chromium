// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Description: Linux specific functionality. Other Linux-derivatives layer on
// top of this translation unit.

#include "base/threading/platform_thread.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <optional>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/process/internal_linux.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread_internal_posix.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_type_delegate.h"
#include "build/build_config.h"

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
    case ThreadType::kDisplayCritical:
    case ThreadType::kRealtimeAudio:
      return cgroup_filepath.Append(FILE_PATH_LITERAL("urgent"));
  }
  NOTREACHED();
}

void SetThreadCgroup(PlatformThreadId thread_id,
                     const FilePath& cgroup_directory) {
  FilePath tasks_filepath = cgroup_directory.Append(FILE_PATH_LITERAL("tasks"));
  std::string tid = NumberToString(thread_id);
  if (!WriteFile(tasks_filepath, as_byte_span(tid))) {
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
    kThreadPriorityToNiceValueMapForTest[7] = {
        {ThreadPriorityForTest::kRealtimeAudio, -10},
        {ThreadPriorityForTest::kDisplay, -8},
        {ThreadPriorityForTest::kNormal, 0},
        {ThreadPriorityForTest::kResourceEfficient, 1},
        {ThreadPriorityForTest::kUtility, 2},
        {ThreadPriorityForTest::kBackground, 10},
};

// These nice values are shared with ChromeOS platform code
// (platform_thread_cros.cc) and have to be unique as ChromeOS has a unique
// type -> nice value mapping.
// The uniqueness of the nice value per-type helps to change and restore the
// scheduling params of threads when their process toggles between FG and BG.
const ThreadTypeToNiceValuePair kThreadTypeToNiceValueMap[7] = {
    {ThreadType::kBackground, 10},       {ThreadType::kUtility, 2},
    {ThreadType::kResourceEfficient, 1}, {ThreadType::kDefault, 0},
    {ThreadType::kDisplayCritical, -8},  {ThreadType::kRealtimeAudio, -10},
};

bool CanSetThreadTypeToRealtimeAudio() {
  // Check if root
  if (geteuid() == 0) {
    return true;
  }

  // A non-zero soft-limit on RLIMIT_RTPRIO is required to be allowed to invoke
  // pthread_setschedparam in SetCurrentThreadTypeForPlatform().
  struct rlimit rlim;
  return getrlimit(RLIMIT_RTPRIO, &rlim) != 0 && rlim.rlim_cur != 0;
}

bool SetCurrentThreadTypeForPlatform(ThreadType thread_type,
                                     MessagePumpType pump_type_hint) {
  const PlatformThreadId thread_id = PlatformThread::CurrentId();

  if (g_thread_type_delegate &&
      g_thread_type_delegate->HandleThreadTypeChange(thread_id, thread_type)) {
    return true;
  }

  internal::SetThreadType(getpid(), thread_id, thread_type, IsViaIPC(false));

  return true;
}

std::optional<ThreadPriorityForTest>
GetCurrentThreadPriorityForPlatformForTest() {
  int maybe_sched_rr = 0;
  struct sched_param maybe_realtime_prio = {0};
  if (pthread_getschedparam(pthread_self(), &maybe_sched_rr,
                            &maybe_realtime_prio) == 0 &&
      maybe_sched_rr == SCHED_RR &&
      maybe_realtime_prio.sched_priority ==
          PlatformThreadLinux::kRealTimeAudioPrio.sched_priority) {
    return std::make_optional(ThreadPriorityForTest::kRealtimeAudio);
  }
  return std::nullopt;
}

}  // namespace internal

// Determine if thread_id is a background thread by looking up whether
// it is in the urgent or non-urgent cpuset.
bool PlatformThreadLinux::IsThreadBackgroundedForTest(
    PlatformThreadId thread_id) {
  FilePath cgroup_filepath(kCgroupDirectory);

  FilePath urgent_cgroup_directory =
      cgroup_filepath.Append(FILE_PATH_LITERAL("cpuset"))
          .Append(FILE_PATH_LITERAL("chrome"))
          .Append(FILE_PATH_LITERAL("urgent"));
  FilePath non_urgent_cgroup_directory =
      cgroup_filepath.Append(FILE_PATH_LITERAL("cpuset"))
          .Append(FILE_PATH_LITERAL("chrome"))
          .Append(FILE_PATH_LITERAL("non-urgent"));

  // Silently ignore request if cgroup directory doesn't exist.
  if (!DirectoryExists(urgent_cgroup_directory) ||
      !DirectoryExists(non_urgent_cgroup_directory)) {
    return false;
  }

  FilePath urgent_tasks_filepath =
      urgent_cgroup_directory.Append(FILE_PATH_LITERAL("tasks"));
  FilePath non_urgent_tasks_filepath =
      non_urgent_cgroup_directory.Append(FILE_PATH_LITERAL("tasks"));

  std::string tid = NumberToString(thread_id);
  // Check if thread_id is in the urgent cpuset
  std::string urgent_tasks;
  if (!ReadFileToString(urgent_tasks_filepath, &urgent_tasks)) {
    return false;
  }
  if (urgent_tasks.find(tid) != std::string::npos) {
    return false;
  }

  // Check if thread_id is in the non-urgent cpuset
  std::string non_urgent_tasks;
  if (!ReadFileToString(non_urgent_tasks_filepath, &non_urgent_tasks)) {
    return false;
  }
  if (non_urgent_tasks.find(tid) != std::string::npos) {
    return true;
  }

  return false;
}

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
                                        ThreadType thread_type,
                                        IsViaIPC via_ipc) {
  internal::SetThreadType(process_id, thread_id, thread_type, via_ipc);
}

namespace internal {
void SetThreadTypeLinux(ProcessId process_id,
                        PlatformThreadId thread_id,
                        ThreadType thread_type,
                        IsViaIPC via_ipc) {
  PlatformThreadLinux::SetThreadCgroupsForThreadType(thread_id, thread_type);

  // Some scheduler syscalls require thread ID of 0 for current thread.
  // This prevents us from requiring to translate the NS TID to
  // global TID.
  PlatformThreadId syscall_tid = thread_id;
  if (thread_id == PlatformThreadLinux::CurrentId()) {
    syscall_tid = 0;
  }

  if (thread_type == ThreadType::kRealtimeAudio) {
    if (sched_setscheduler(syscall_tid, SCHED_RR,
                           &PlatformThreadLinux::kRealTimeAudioPrio) == 0) {
      return;
    }
    // If failed to set to RT, fallback to setpriority to set nice value.
    DPLOG(ERROR) << "Failed to set realtime priority for thread " << thread_id;
  }

  const int nice_setting = ThreadTypeToNiceValue(thread_type);
  if (setpriority(PRIO_PROCESS, static_cast<id_t>(syscall_tid), nice_setting)) {
    DVPLOG(1) << "Failed to set nice value of thread (" << thread_id << ") to "
              << nice_setting;
  }
}

}  // namespace internal

}  // namespace base
