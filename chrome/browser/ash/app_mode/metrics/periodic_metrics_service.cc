// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/metrics/periodic_metrics_service.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/process/process_iterator.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"

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
const char kKioskDiskUsagePercentageHistogram[] = "Kiosk.DiskUsagePercentage";
const char kKioskChromeProcessCountHistogram[] = "Kiosk.ChromeProcessCount";

const base::TimeDelta kPeriodicMetricsInterval = base::Hours(1);

// This class is calculating amount of available and total disk space and
// reports the percentage of available disk space to the histogram. Since the
// calculation contains a blocking call, this is done asynchronously.
class DiskSpaceCalculator {
 public:
  struct DiskSpaceInfo {
    int64_t free_bytes;
    int64_t total_bytes;
  };
  void StartCalculation() {
    base::FilePath path;
    DCHECK(base::PathService::Get(base::DIR_HOME, &path));
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&DiskSpaceCalculator::GetDiskSpaceBlocking, path),
        base::BindOnce(&DiskSpaceCalculator::OnReceived,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  static DiskSpaceInfo GetDiskSpaceBlocking(const base::FilePath& mount_path) {
    int64_t free_bytes = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
    int64_t total_bytes = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
    return DiskSpaceInfo{free_bytes, total_bytes};
  }

 private:
  void OnReceived(const DiskSpaceInfo& disk_info) {
    ReportUsedPercentage(kKioskDiskUsagePercentageHistogram,
                         disk_info.free_bytes, disk_info.total_bytes);
  }

  base::WeakPtrFactory<DiskSpaceCalculator> weak_ptr_factory_{this};
};

PeriodicMetricsService::PeriodicMetricsService()
    : disk_space_calculator_(std::make_unique<DiskSpaceCalculator>()) {}

PeriodicMetricsService::~PeriodicMetricsService() = default;

void PeriodicMetricsService::StartRecordingPeriodicMetrics() {
  // Record all periodic metrics at the beginning of the kiosk session and then
  // every `kPeriodicMetricsInterval`.
  RecordPeriodicMetrics();
  metrics_timer_.Start(FROM_HERE, kPeriodicMetricsInterval, this,
                       &PeriodicMetricsService::RecordPeriodicMetrics);
}

void PeriodicMetricsService::RecordPeriodicMetrics() {
  RecordRamUsage();
  RecordSwapUsage();
  RecordDiskSpaceUsage();
  RecordChromeProcessCount();
}

void PeriodicMetricsService::RecordRamUsage() const {
  int64_t available_ram = base::SysInfo::AmountOfAvailablePhysicalMemory();
  int64_t total_ram = base::SysInfo::AmountOfPhysicalMemory();
  ReportUsedPercentage(kKioskRamUsagePercentageHistogram, available_ram,
                       total_ram);
}

void PeriodicMetricsService::RecordSwapUsage() const {
  base::SystemMemoryInfoKB memory;
  if (!base::GetSystemMemoryInfo(&memory)) {
    return;
  }
  int64_t swap_free = memory.swap_free;
  int64_t swap_total = memory.swap_total;
  ReportUsedPercentage(kKioskSwapUsagePercentageHistogram, swap_free,
                       swap_total);
}

void PeriodicMetricsService::RecordDiskSpaceUsage() const {
  DCHECK(disk_space_calculator_);
  disk_space_calculator_->StartCalculation();
}

void PeriodicMetricsService::RecordChromeProcessCount() const {
  base::FilePath chrome_path;
  DCHECK(base::PathService::Get(base::FILE_EXE, &chrome_path));
  base::FilePath::StringType exe_name = chrome_path.BaseName().value();
  int process_count = base::GetProcessCount(exe_name, nullptr);
  base::UmaHistogramCounts100(kKioskChromeProcessCountHistogram, process_count);
}

}  // namespace ash
