// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crosvm_metrics.h"

#include <unistd.h>

#include <cmath>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/crostini/crosvm_process_list.h"

namespace crostini {

namespace {

constexpr char kCrosvmProcessesHistogram[] = "Crostini.Crosvm.Processes.Count";
constexpr char kCrosvmCpuPercentageHistogram[] =
    "Crostini.Crosvm.CpuPercentage";
constexpr char kCrosvmRssPercentageHistogram[] =
    "Crostini.Crosvm.RssPercentage";

constexpr base::TimeDelta kCrosvmMetricsInterval =
    base::TimeDelta::FromMinutes(10);

}  // namespace

CrosvmMetrics::CrosvmMetrics()
    : task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      page_size_(sysconf(_SC_PAGESIZE)) {}

CrosvmMetrics::~CrosvmMetrics() = default;

void CrosvmMetrics::Start() {
  task_runner_->PostNonNestableTask(
      FROM_HERE, base::BindOnce(&CrosvmMetrics::CollectCycleStartData,
                                weak_ptr_factory_.GetWeakPtr()));
  timer_.Start(FROM_HERE, kCrosvmMetricsInterval, this,
               &CrosvmMetrics::MetricsCycleCallback);
}

void CrosvmMetrics::CollectCycleStartData() {
  previous_pid_stat_map_ = GetCrosvmPidStatMap();
  base::Optional<int64_t> total_cpu_time =
      chromeos::system::GetCpuTimeJiffies();
  if (!total_cpu_time.has_value()) {
    cycle_start_data_collected_ = false;
    return;
  }
  previous_total_cpu_time_ = total_cpu_time.value();
  cycle_start_data_collected_ = true;
}

// static
int CrosvmMetrics::CalculateCrosvmRssPercentage(const PidStatMap& pid_stat_map,
                                                int64_t mem_used,
                                                int64_t page_size) {
  int64_t total_rss = 0;
  for (const auto& pair : pid_stat_map) {
    total_rss += pair.second.rss;
  }
  return std::lround(static_cast<double>(total_rss) /
                     (mem_used * 1024 / page_size) * 100);
}

// static
int CrosvmMetrics::CalculateCrosvmCpuPercentage(
    const PidStatMap& pid_stat_map,
    const PidStatMap& previous_pid_stat_map,
    int64_t cycle_cpu_time) {
  int64_t total_cpu_time = 0;
  for (const auto& pair : pid_stat_map) {
    auto it = previous_pid_stat_map.find(pair.first);
    if (it == previous_pid_stat_map.end()) {
      total_cpu_time += pair.second.utime + pair.second.stime;
    } else {
      total_cpu_time += (pair.second.utime + pair.second.stime) -
                        (it->second.utime + it->second.stime);
    }
  }
  return std::lround(static_cast<double>(total_cpu_time) / cycle_cpu_time *
                     100);
}

void CrosvmMetrics::MetricsCycleCallback() {
  task_runner_->PostNonNestableTask(
      FROM_HERE, base::BindOnce(&CrosvmMetrics::MetricsCycle,
                                weak_ptr_factory_.GetWeakPtr()));
}

void CrosvmMetrics::MetricsCycle() {
  if (!cycle_start_data_collected_) {
    CollectCycleStartData();
    return;
  }

  PidStatMap pid_stat_map = GetCrosvmPidStatMap();
  base::Optional<int64_t> total_cpu_time =
      chromeos::system::GetCpuTimeJiffies();
  if (!total_cpu_time.has_value()) {
    cycle_start_data_collected_ = false;
    return;
  }
  int64_t cycle_cpu_time = total_cpu_time.value() - previous_total_cpu_time_;
  base::Optional<int64_t> mem_used = chromeos::system::GetUsedMemTotalKB();
  if (!mem_used.has_value()) {
    cycle_start_data_collected_ = false;
    return;
  }

  if (pid_stat_map.empty()) {
    previous_pid_stat_map_ = pid_stat_map;
    previous_total_cpu_time_ = total_cpu_time.value();
    return;
  }

  int rss_percentage =
      CalculateCrosvmRssPercentage(pid_stat_map, mem_used.value(), page_size_);
  int cpu_percentage = CalculateCrosvmCpuPercentage(
      pid_stat_map, previous_pid_stat_map_, cycle_cpu_time);
  UMA_HISTOGRAM_COUNTS_100(kCrosvmProcessesHistogram, pid_stat_map.size());
  UMA_HISTOGRAM_PERCENTAGE(kCrosvmCpuPercentageHistogram, cpu_percentage);
  UMA_HISTOGRAM_PERCENTAGE(kCrosvmRssPercentageHistogram, rss_percentage);
  previous_pid_stat_map_ = pid_stat_map;
  previous_total_cpu_time_ = total_cpu_time.value();
}

}  // namespace crostini
