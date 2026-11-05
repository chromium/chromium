// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_iterator.h"

#include <stddef.h>

#include <string_view>

#include "base/check_is_test.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/internal_linux.h"
#include "base/process/process_handle.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"

namespace base {

class ScopedAllowBlockingForProc : public ScopedAllowBlocking {};

namespace {

// Reads <proc_dir>/<pid>/cmdline and populates |proc_cmd_line_args| with the
// command line arguments. Returns true if successful.
// Note: /proc/<pid>/cmdline contains command line arguments separated by single
// null characters. We tokenize it into a vector of strings using '\0' as a
// delimiter.
bool GetProcCmdline(pid_t pid,
                    const FilePath& proc_dir,
                    std::vector<std::string>* proc_cmd_line_args) {
  // Synchronously reading files in /proc is safe, and non-/proc paths are only
  // used in tests.
  ScopedAllowBlockingForProc allow_blocking;

  FilePath cmd_line_file =
      internal::GetProcPidDir(pid, proc_dir).Append("cmdline");
  std::string cmd_line;
  if (!ReadFileToString(cmd_line_file, &cmd_line)) {
    return false;
  }
  std::string delimiters;
  delimiters.push_back('\0');
  *proc_cmd_line_args =
      SplitString(cmd_line, delimiters, KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY);
  return true;
}

}  // namespace

ProcessIterator::ProcessIterator(const ProcessFilter* filter)
    : ProcessIterator(filter, FilePath(internal::kProcDir)) {}

ProcessIterator::ProcessIterator(const ProcessFilter* filter,
                                 const FilePath& proc_dir)
    : procfs_dir_(opendir(proc_dir.value().c_str())),
      procfs_dir_path_(proc_dir),
      filter_(filter) {
  if (proc_dir.value() != internal::kProcDir) {
    CHECK_IS_TEST();
  }
  if (!procfs_dir_) {
    // On Android, SELinux may prevent reading /proc. See
    // https://crbug.com/581517 for details.
    PLOG(ERROR) << "opendir " << proc_dir;
  }
}

ProcessIterator::~ProcessIterator() = default;

bool ProcessIterator::CheckForNextProcess() {
  // TODO(port): skip processes owned by different UID

  if (!procfs_dir_) {
    DLOG(ERROR) << "Skipping CheckForNextProcess(), no procfs_dir_";
    return false;
  }

  pid_t pid = kNullProcessId;
  std::vector<std::string> cmd_line_args;
  std::string stats_data;
  std::vector<std::string_view> proc_stats;

  while (true) {
    dirent* const slot = readdir(procfs_dir_.get());
    // all done looking through /proc?
    if (!slot) {
      return false;
    }

    // If not a process, keep looking for one.
    pid = internal::ProcDirSlotToPid(slot->d_name);
    if (!pid) {
      continue;
    }

    if (!internal::ReadProcStats(pid, procfs_dir_path_, &stats_data)) {
      continue;
    }
    if (!internal::ParseProcStats(stats_data, &proc_stats)) {
      continue;
    }

    std::string_view runstate = proc_stats.at(internal::VM_STATE);
    if (runstate.size() != 1) {
      NOTREACHED();
    }

    // Is the process in 'Zombie' state, i.e. dead but waiting to be reaped?
    // Allowed values: D R S T Z
    if (runstate[0] == 'Z') {
      // Nope, it's a zombie; somebody isn't cleaning up after their children.
      // (e.g. WaitForProcessesToExit doesn't clean up after dead children yet.)
      // There could be a lot of zombies, can't really decrement i here.
      continue;
    }

    // Read the command-line args. Do this last to avoid useless string copies.
    if (GetProcCmdline(pid, procfs_dir_path_, &cmd_line_args)) {
      break;
    }
  }

  entry_.pid_ = pid;
  entry_.ppid_ = checked_cast<ProcessId>(
      GetProcStatsFieldAsInt64(proc_stats, internal::VM_PPID));
  entry_.gid_ = checked_cast<ProcessId>(
      GetProcStatsFieldAsInt64(proc_stats, internal::VM_PGRP));
  entry_.cmd_line_args_.assign(cmd_line_args.begin(), cmd_line_args.end());
  entry_.exe_file_ =
      GetProcessExecutablePath(pid, procfs_dir_path_).BaseName().value();
  return true;
}

bool NamedProcessIterator::IncludeEntry() {
  if (executable_name_ != entry().exe_file()) {
    return false;
  }
  return ProcessIterator::IncludeEntry();
}

}  // namespace base
