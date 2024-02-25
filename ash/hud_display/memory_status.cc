// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/memory_status.h"

#include <unistd.h>

#include <string_view>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/internal_linux.h"
#include "base/process/process_iterator.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_restrictions.h"

namespace ash {
namespace hud_display {
namespace {

constexpr char kProcDir[] = "/proc";
constexpr char kSysFsCgroupCpuDir[] = "/sys/fs/cgroup/cpu";

// Fields from /proc/<pid>/statm, 0-based. See man 5 proc.
// If the ordering ever changes, carefully review functions that use these
// values.
enum class ProcStatMFields {
  VM_SIZE = 0,    // Virtual memory size in bytes.
  VM_RSS = 1,     // Resident Set Size in pages.
  VM_SHARED = 2,  // number of resident shared pages
};

base::FilePath GetProcPidDir(pid_t pid) {
  return base::FilePath(kProcDir).Append(base::NumberToString(pid));
}

std::string ReadProcFile(const base::FilePath& path) {
  std::string result;
  ReadFileToString(path, &result);
  return result;
}

// Reads and returns /proc/<pid>/cmdline
// Note: /proc/<pid>/cmdline contains command line arguments separated by single
// null characters.
std::string GetProcCmdline(pid_t pid) {
  return ReadProcFile(GetProcPidDir(pid).Append("cmdline"));
}

int64_t GetProcVM_RSS(pid_t pid) {
  const std::string statm = ReadProcFile(GetProcPidDir(pid).Append("statm"));
  const std::vector<std::string_view> parts = base::SplitStringPiece(
      statm, " \n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (parts.size() <= static_cast<size_t>(ProcStatMFields::VM_RSS)) {
    DLOG(ERROR) << "GetProcVM_RSS(): No data in '" << statm << "'!";
    return 0;
  }
  int64_t result;
  base::StringToInt64(parts[static_cast<size_t>(ProcStatMFields::VM_RSS)],
                      &result);
  return result * getpagesize();
}

int64_t GetProcVM_SHARED(pid_t pid) {
  const std::string statm = ReadProcFile(GetProcPidDir(pid).Append("statm"));
  const std::vector<std::string_view> parts = base::SplitStringPiece(
      statm, " \n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (parts.size() <= static_cast<size_t>(ProcStatMFields::VM_SHARED)) {
    DLOG(ERROR) << "GetProcVM_SHARED(): No data!";
    return 0;
  }
  int64_t result;
  base::StringToInt64(parts[static_cast<size_t>(ProcStatMFields::VM_SHARED)],
                      &result);
  return result * getpagesize();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

// ProcessMemoryCountersByFlag
MemoryStatus::ProcessMemoryCountersByFlag::ProcessMemoryCountersByFlag(
    const std::string& cmd_line_flag)
    : flag_(cmd_line_flag) {}
MemoryStatus::ProcessMemoryCountersByFlag::~ProcessMemoryCountersByFlag() =
    default;

bool MemoryStatus::ProcessMemoryCountersByFlag::TryRead(
    const base::ProcessId& pid,
    const std::string& cmdline) {
  if (cmdline.find(flag_) == std::string::npos)
    return false;

  rss_ += GetProcVM_RSS(pid);
  rss_shared_ += GetProcVM_SHARED(pid);
  return true;
}

// ProcessMemoryCountersByCgroup
MemoryStatus::ProcessMemoryCountersByCgroup::ProcessMemoryCountersByCgroup(
    const std::string& expected_cgroup) {
  const base::FilePath pids_filename = base::FilePath(kSysFsCgroupCpuDir)
                                           .Append(expected_cgroup)
                                           .Append("cgroup.procs");
  const std::string pids_list_str = ReadProcFile(pids_filename);
  if (pids_list_str.empty()) {
    // Ignore read failures.
    return;
  }
  const std::vector<std::string_view> pids = base::SplitStringPiece(
      pids_list_str, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& p : pids) {
    int64_t pid;
    if (base::StringToInt64(p, &pid))
      pids_.insert(pid);
  }
}

MemoryStatus::ProcessMemoryCountersByCgroup::~ProcessMemoryCountersByCgroup() =
    default;

bool MemoryStatus::ProcessMemoryCountersByCgroup::TryRead(
    const base::ProcessId& pid) {
  if (!pids_.contains(pid))
    return false;

  rss_ += GetProcVM_RSS(pid);
  rss_shared_ += GetProcVM_SHARED(pid);
  return true;
}

// MemoryStatus
MemoryStatus::MemoryStatus() {
  UpdatePerProcessStat();
  UpdateMeminfo();
}

void MemoryStatus::UpdatePerProcessStat() {
  // TODO: Can we remember process status in some way?
  base::ProcessIterator process_iter(/*filter=*/nullptr);
  while (const base::ProcessEntry* process_entry =
             process_iter.NextProcessEntry()) {
    const base::Process process(process_entry->pid());
    if (process.is_current()) {
      browser_rss_ = GetProcVM_RSS(process.Pid());
      browser_rss_shared_ = GetProcVM_SHARED(process.Pid());
      continue;
    }
    const std::string cmdline = GetProcCmdline(process.Pid());
    if (gpu_.TryRead(process.Pid(), cmdline) ||
        renderers_.TryRead(process.Pid(), cmdline)) {
      continue;
    }
    arc_.TryRead(process.Pid());
  }
}

void MemoryStatus::UpdateMeminfo() {
  base::SystemMemoryInfoKB meminfo;
  base::GetSystemMemoryInfo(&meminfo);
  total_ram_size_ = meminfo.total * 1024LL;
  total_free_ = meminfo.free * 1024LL;

  base::GraphicsMemoryInfoKB gpu_meminfo;
  if (base::GetGraphicsMemoryInfo(&gpu_meminfo))
    gpu_kernel_ = gpu_meminfo.gpu_memory_size;
  else
    gpu_kernel_ = 0LL;
}

}  // namespace hud_display
}  // namespace ash
