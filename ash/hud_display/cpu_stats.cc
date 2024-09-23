// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/cpu_stats.h"

#include <cinttypes>
#include <cstdio>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"

namespace ash {
namespace hud_display {
namespace {

constexpr char kProcStatFile[] = "/proc/stat";

std::string ReadProcFile(const base::FilePath& path) {
  std::string result;
  base::ReadFileToString(path, &result);
  return result;
}

}  // namespace

CpuStats GetProcStatCPU() {
  const std::string stat = ReadProcFile(base::FilePath(kProcStatFile));
  // First string should be total Cpu statistics.
  CHECK(base::StartsWith(stat, "cpu ", base::CompareCase::SENSITIVE));
  const size_t newline_pos = stat.find('\n');
  if (newline_pos == std::string::npos) {
    NOTREACHED();
  }

  // Parse first line only.
  // Format is described in [man 5 proc] and in kernel source proc/stat.c .
  // https://github.com/torvalds/linux/blob/v5.11/fs/proc/stat.c#L153-L163

  CpuStats stats;
  int assigned =
      sscanf(stat.c_str(),
             "cpu %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
             " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 "",
             &stats.user, &stats.nice, &stats.system, &stats.idle,
             &stats.iowait, &stats.irq, &stats.softirq, &stats.steal,
             &stats.guest, &stats.guest_nice);
  DCHECK_EQ(assigned, 10);
  return stats;
}

}  // namespace hud_display
}  // namespace ash
