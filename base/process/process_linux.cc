// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include <errno.h>
#include <linux/magic.h>
#include <sys/resource.h>
#include <sys/vfs.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/posix/can_lower_nice_to.h"
#include "base/process/internal_linux.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/metrics/field_trial_params.h"
#include "base/process/process_priority_delegate.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace base {

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kFlattenCpuCgroups, FEATURE_ENABLED_BY_DEFAULT);

// If FlattenCpuCgroupsUnified parameter is enabled, foreground renderer
// processes uses /sys/fs/cgroup/cpu/ui cgroup instead of
// /sys/fs/cgroup/cpu/chrome_renderers sharing the cpu cgroup with the browser
// process and others.
BASE_FEATURE_PARAM(bool,
                   kFlattenCpuCgroupsUnified,
                   &kFlattenCpuCgroups,
                   "unified_cpu_cgroup",
                   false);
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

const int kForegroundPriority = 0;

#if BUILDFLAG(IS_CHROMEOS)
ProcessPriorityDelegate* g_process_priority_delegate = nullptr;

// We are more aggressive in our lowering of background process priority
// for chromeos as we have much more control over other processes running
// on the machine.
//
// TODO(davemoore) Refactor this by adding support for higher levels to set
// the foregrounding / backgrounding process so we don't have to keep
// chrome / chromeos specific logic here.
const int kBackgroundPriority = 19;
const char kControlPath[] = "/sys/fs/cgroup/cpu%s/cgroup.procs";
const char kForeground[] = "/chrome_renderers/foreground";
const char kBackground[] = "/chrome_renderers/background";
const char kForegroundExperiment[] = "/chrome_renderers";
const char kForegroundUnifiedExperiment[] = "/ui";
const char kBackgroundExperiment[] = "/chrome_renderers_background";
const char kProcPath[] = "/proc/%d/cgroup";

bool PathIsCGroupFileSystem(const FilePath& path) {
  struct statfs statfs_buf;
  if (statfs(path.value().c_str(), &statfs_buf) < 0) {
    return false;
  }
  return statfs_buf.f_type == CGROUP_SUPER_MAGIC;
}

struct CGroups {
  // Check for cgroups files. ChromeOS supports these by default. It creates
  // a cgroup mount in /sys/fs/cgroup and then configures two cpu task groups,
  // one contains at most a single foreground renderer and the other contains
  // all background renderers. This allows us to limit the impact of background
  // renderers on foreground ones to a greater level than simple renicing.
  bool enabled;
  FilePath foreground_file;
  FilePath background_file;

  CGroups() {
    if (FeatureList::IsEnabled(kFlattenCpuCgroups)) {
      foreground_file =
          FilePath(StringPrintf(kControlPath, kFlattenCpuCgroupsUnified.Get()
                                                  ? kForegroundUnifiedExperiment
                                                  : kForegroundExperiment));
      background_file =
          FilePath(StringPrintf(kControlPath, kBackgroundExperiment));
    } else {
      foreground_file = FilePath(StringPrintf(kControlPath, kForeground));
      background_file = FilePath(StringPrintf(kControlPath, kBackground));
    }
    enabled = PathIsCGroupFileSystem(foreground_file) &&
              PathIsCGroupFileSystem(background_file);
  }

  static CGroups& Get() {
    static auto& groups = *new CGroups;
    return groups;
  }
};

#else
const int kBackgroundPriority = 5;
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

Time Process::CreationTime() const {
  int64_t start_ticks = is_current()
                            ? internal::ReadProcSelfStatsAndGetFieldAsInt64(
                                  internal::VM_STARTTIME)
                            : internal::ReadProcStatsAndGetFieldAsInt64(
                                  Pid(), internal::VM_STARTTIME);

  if (!start_ticks) {
    return Time();
  }

  TimeDelta start_offset = internal::ClockTicksToTimeDelta(start_ticks);
  Time boot_time = internal::GetBootTime();
  if (boot_time.is_null()) {
    return Time();
  }
  return Time(boot_time + start_offset);
}

// static
bool Process::CanSetPriority() {
#if BUILDFLAG(IS_CHROMEOS)
  if (g_process_priority_delegate) {
    return g_process_priority_delegate->CanSetProcessPriority();
  }

  if (CGroups::Get().enabled) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  static const bool can_reraise_priority =
      internal::CanLowerNiceTo(kForegroundPriority);
  return can_reraise_priority;
}

Process::Priority Process::GetPriority() const {
  DCHECK(IsValid());

#if BUILDFLAG(IS_CHROMEOS)
  if (g_process_priority_delegate) {
    return g_process_priority_delegate->GetProcessPriority(process_);
  }

  if (CGroups::Get().enabled) {
    // Used to allow reading the process priority from proc on thread launch.
    ScopedAllowBlocking scoped_allow_blocking;
    std::string proc;
    if (ReadFileToString(FilePath(StringPrintf(kProcPath, process_)), &proc)) {
      return GetProcessPriorityCGroup(proc);
    }
    return Priority::kUserBlocking;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return GetOSPriority() == kBackgroundPriority ? Priority::kBestEffort
                                                : Priority::kUserBlocking;
}

bool Process::SetPriority(Priority priority) {
  DCHECK(IsValid());

#if BUILDFLAG(IS_CHROMEOS)
  if (g_process_priority_delegate) {
    return g_process_priority_delegate->SetProcessPriority(process_, priority);
  }

  if (CGroups::Get().enabled) {
    std::string pid = NumberToString(process_);
    const FilePath file = priority == Priority::kBestEffort
                              ? CGroups::Get().background_file
                              : CGroups::Get().foreground_file;
    return WriteFile(file, pid);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (!CanSetPriority()) {
    return false;
  }

  int priority_value = priority == Priority::kBestEffort ? kBackgroundPriority
                                                         : kForegroundPriority;
  int result =
      setpriority(PRIO_PROCESS, static_cast<id_t>(process_), priority_value);
  DPCHECK(result == 0);
  return result == 0;
}

#if BUILDFLAG(IS_CHROMEOS)
Process::Priority GetProcessPriorityCGroup(std::string_view cgroup_contents) {
  // The process can be part of multiple control groups, and for each cgroup
  // hierarchy there's an entry in the file. We look for a control group
  // named "/chrome_renderers/background" to determine if the process is
  // backgrounded. crbug.com/548818.
  std::vector<std::string_view> lines = SplitStringPiece(
      cgroup_contents, "\n", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
  for (const auto& line : lines) {
    std::vector<std::string_view> fields =
        SplitStringPiece(line, ":", TRIM_WHITESPACE, SPLIT_WANT_ALL);
    if (fields.size() != 3U) {
      NOTREACHED();
    }
    if (fields[2] == kBackgroundExperiment || fields[2] == kBackground) {
      return Process::Priority::kBestEffort;
    }
  }

  return Process::Priority::kUserBlocking;
}

// Reads /proc/<pid>/status and returns the PID in its PID namespace.
// If the process is not in a PID namespace or /proc/<pid>/status does not
// report NSpid, kNullProcessId is returned.
ProcessId Process::GetPidInNamespace() const {
  std::string buffer;
  std::optional<StringViewPairs> pairs =
      internal::ReadProcFileToTrimmedStringPairs(process_, "status", &buffer);
  if (!pairs) {
    return kNullProcessId;
  }
  for (const auto& [key, value_str] : *pairs) {
    if (key == "NSpid") {
      std::vector<std::string_view> split_value_str = SplitStringPiece(
          value_str, "\t", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
      if (split_value_str.size() <= 1) {
        return kNullProcessId;
      }
      int value;
      // The last value in the list is the PID in the namespace.
      if (!StringToInt(split_value_str.back(), &value)) {
        NOTREACHED();
      }
      return value;
    }
  }
  return kNullProcessId;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
bool Process::IsSeccompSandboxed() {
  uint64_t seccomp_value = 0;
  if (!internal::ReadProcStatusAndGetFieldAsUint64(process_, "Seccomp",
                                                   &seccomp_value)) {
    return false;
  }
  return seccomp_value > 0;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
// static
void Process::SetProcessPriorityDelegate(ProcessPriorityDelegate* delegate) {
  // A component cannot override a delegate set by another component, thus
  // disallow setting a delegate when one already exists.
  DCHECK_NE(!!g_process_priority_delegate, !!delegate);

  g_process_priority_delegate = delegate;
}

void Process::InitializePriority() {
  if (g_process_priority_delegate) {
    g_process_priority_delegate->InitializeProcessPriority(process_);
    return;
  }
}

void Process::ForgetPriority() {
  if (g_process_priority_delegate) {
    g_process_priority_delegate->ForgetProcessPriority(process_);
    return;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace base
