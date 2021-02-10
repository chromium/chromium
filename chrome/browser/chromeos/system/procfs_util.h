// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_PROCFS_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_PROCFS_UTIL_H_

#include "base/files/file_path.h"
#include "base/values.h"

namespace chromeos {
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
base::Optional<SingleProcStat> GetSingleProcStat(
    const base::FilePath& stat_file);

// Returns the total CPU time used in jiffies by reading /proc/stat file.
// The input |stat_file| is used for testing.
base::Optional<int64_t> GetCpuTimeJiffies(
    const base::FilePath& stat_file = base::FilePath("/proc/stat"));

// Returns the total system memory used at the moment in kBs by reading
// /proc/meminfo file.
// The input |meminfo_file| is used for testing.
base::Optional<int64_t> GetUsedMemTotalKB(
    const base::FilePath& meminfo_file = base::FilePath("/proc/meminfo"));

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_PROCFS_UTIL_H_
