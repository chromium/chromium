// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/metrics/periodic_metrics_service.h"
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"

namespace ash {

namespace {

void ReportUsedPercentage(const char* histogram_name,
                          int64_t available,
                          int64_t total) {
  int percents;
  if (total <= 0 || available < 0 || total < available) {
    percents = 100;
  } else {
    percents = (total - available) * 100 / total;
  }
  base::UmaHistogramPercentage(histogram_name, percents);
}

}  // namespace

const char kKioskRamUsagePercentageHistogram[] = "Kiosk.RamUsagePercentage";
const char kKioskSwapUsagePercentageHistogram[] = "Kiosk.SwapUsagePercentage";

const base::TimeDelta kPeriodicMetricsInterval = base::Hours(1);

PeriodicMetricsService::PeriodicMetricsService() = default;

PeriodicMetricsService::~PeriodicMetricsService() = default;

void PeriodicMetricsService::StartRecordingPeriodicMetrics() {
  // Record all periodic metrics at the beginning of the kiosk session and then
  // every `kPeriodicMetricsInterval`.
  RecordPeriodicMetrics();
  metrics_timer_.Start(
      FROM_HERE, kPeriodicMetricsInterval,
      base::BindRepeating(&PeriodicMetricsService::RecordPeriodicMetrics,
                          weak_ptr_factory_.GetWeakPtr()));
}

void PeriodicMetricsService::RecordPeriodicMetrics() {
  RecordRamUsage();
  RecordSwapUsage();
}

void PeriodicMetricsService::RecordRamUsage() const {
  int64_t available_ram =
      base::SysInfo::AmountOfAvailablePhysicalMemory().InBytes();
  int64_t total_ram = base::SysInfo::AmountOfPhysicalMemory().InBytes();
  ReportUsedPercentage(kKioskRamUsagePercentageHistogram, available_ram,
                       total_ram);
}

void PeriodicMetricsService::RecordSwapUsage() const {
  base::SystemMemoryInfo memory;
  if (!base::GetSystemMemoryInfo(&memory)) {
    return;
  }
  int64_t swap_free = memory.swap_free.InKiB();
  int64_t swap_total = memory.swap_total.InKiB();
  ReportUsedPercentage(kKioskSwapUsagePercentageHistogram, swap_free,
                       swap_total);
}

}  // namespace ash
