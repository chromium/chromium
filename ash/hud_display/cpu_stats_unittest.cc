// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/cpu_stats.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::hud_display {
namespace {

TEST(CpuStatsTest, ReadsAndParsesAggregateCpuLine) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath proc_stat_path = temp_dir.GetPath().AppendASCII("stat");
  constexpr std::string_view kProcStat =
      "cpu  101\t102 103 104 105 106 107 108 109 110\n"
      "cpu0 201 202 203 204 205 206 207 208 209 210\n";
  ASSERT_TRUE(base::WriteFile(proc_stat_path, kProcStat));

  const CpuStats stats = internal::ReadProcStatCPU(proc_stat_path);

  EXPECT_EQ(stats.user, 101u);
  EXPECT_EQ(stats.nice, 102u);
  EXPECT_EQ(stats.system, 103u);
  EXPECT_EQ(stats.idle, 104u);
  EXPECT_EQ(stats.iowait, 105u);
  EXPECT_EQ(stats.irq, 106u);
  EXPECT_EQ(stats.softirq, 107u);
  EXPECT_EQ(stats.steal, 108u);
  EXPECT_EQ(stats.guest, 109u);
  EXPECT_EQ(stats.guest_nice, 110u);
}

}  // namespace
}  // namespace ash::hud_display
