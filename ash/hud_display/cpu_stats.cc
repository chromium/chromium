// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/cpu_stats.h"

#include <array>
#include <string_view>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
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

CpuStats ParseProcStatCPU(std::string_view stat) {
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
  const auto fields = std::to_array<uint64_t*>(
      {&stats.user, &stats.nice, &stats.system, &stats.idle, &stats.iowait,
       &stats.irq, &stats.softirq, &stats.steal, &stats.guest,
       &stats.guest_nice});
  const std::string_view first_line = stat.substr(0, newline_pos);
  base::StringViewTokenizer tokenizer(first_line, base::kWhitespaceASCII);
  CHECK(tokenizer.GetNext());
  CHECK_EQ(tokenizer.token_piece(), "cpu");
  for (uint64_t* field : fields) {
    CHECK(tokenizer.GetNext());
    CHECK(base::StringToUint64(tokenizer.token_piece(), field));
  }
  return stats;
}

}  // namespace

namespace internal {

CpuStats ReadProcStatCPU(const base::FilePath& path) {
  return ParseProcStatCPU(ReadProcFile(path));
}

}  // namespace internal

CpuStats GetProcStatCPU() {
  return internal::ReadProcStatCPU(base::FilePath(kProcStatFile));
}

}  // namespace hud_display
}  // namespace ash
