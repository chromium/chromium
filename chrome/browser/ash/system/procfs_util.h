// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_PROCFS_UTIL_H_
#define CHROME_BROWSER_ASH_SYSTEM_PROCFS_UTIL_H_

#include <optional>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/process/process_handle.h"

namespace ash {
namespace system {

// A single process data from a /proc/[pid]/stat file.
struct SingleProcStat {
  // Process PID.
  pid_t pid = -1;
  // Process name.
  std::string name;
  // Parent process PID.
  pid_t ppid = -1;
  // Scheduled time in user mode in jiffies.
  int64_t utime = 0;
  // Scheduled time in kernel mode in jiffies.
  int64_t stime = 0;
  // Number of pages in real memory.
  int64_t rss = 0;

  bool operator==(const SingleProcStat& rhs) const {
    if (this->pid != rhs.pid)
      return false;
    if (this->name != rhs.name)
      return false;
    if (this->ppid != rhs.ppid)
      return false;
    if (this->utime != rhs.utime)
      return false;
    if (this->stime != rhs.stime)
      return false;
    if (this->rss != rhs.rss)
      return false;
    return true;
  }
};

// Returns a single process information by reading a /proc/[pid]stat file.
std::optional<SingleProcStat> GetSingleProcStat(
    const base::FilePath& stat_file);

// Returns the total CPU time used in jiffies by reading /proc/stat file.
// The input |stat_file| is used for testing.
std::optional<int64_t> GetCpuTimeJiffies(
    const base::FilePath& stat_file = base::FilePath("/proc/stat"));

// A file object for "/proc/<pid>/stat".
class ProcStatFile {
 public:
  explicit ProcStatFile(base::ProcessId process_id);
  ProcStatFile(ProcStatFile&&) = default;
  ProcStatFile(ProcStatFile&) = delete;
  ~ProcStatFile();

  // Returns whether the stat file is valid. See `base::File::IsValid()` for
  // details.
  bool IsValid() const;

  // Returns whether the process is still alive or not.
  bool IsPidAlive();

 private:
  base::File file_;
};

}  // namespace system
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_PROCFS_UTIL_H_
