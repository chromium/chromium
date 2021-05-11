// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/chromeos/crostini/crosvm_metrics.h"

namespace crostini {

TEST(CrosvmMetricsTest, CalculateCrosvmRssPercentageSuccess) {
  CrosvmMetrics::PidStatMap pid_proc_map = {
      {1111, {.pid = 1111, .utime = 1, .stime = 1, .rss = 20}}};
  int64_t mem_used = 1000;
  int64_t page_size = 512;
  int expected = 1;
  EXPECT_EQ(expected, CrosvmMetrics::CalculateCrosvmRssPercentage(
                          pid_proc_map, mem_used, page_size));
}

TEST(CrosvmMetricsTest, CalculateCrosvmCpuPercentageSuccess) {
  CrosvmMetrics::PidStatMap previous_pid_stat_map = {
      {1111, {.pid = 1111, .utime = 1, .stime = 0, .rss = 20}}};
  CrosvmMetrics::PidStatMap pid_stat_map = {
      {1111, {.pid = 1111, .utime = 6, .stime = 4, .rss = 20}}};
  int64_t cycle_cpu_time = 1000;
  int expected = 1;
  EXPECT_EQ(expected, CrosvmMetrics::CalculateCrosvmCpuPercentage(
                          pid_stat_map, previous_pid_stat_map, cycle_cpu_time));
}

}  // namespace crostini
