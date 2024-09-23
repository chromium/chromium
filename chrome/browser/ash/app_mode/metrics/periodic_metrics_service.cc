// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/metrics/periodic_metrics_service.h"
#include <string>

#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/process/process_iterator.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/user_activity/user_activity_detector.h"

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

bool IsDeviceOnline() {
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!default_network) {
    // No connected network.
    return false;
  }
  return default_network->IsOnline();
}

base::TimeDelta GetDeviceIdlePeriod() {
  return base::TimeTicks::Now() -
         ui::UserActivityDetector::Get()->last_activity_time();
}

template <typename ResultType>
std::optional<ResultType> GetSavedEnumFromKioskMetrics(
    const PrefService* prefs,
    const std::string& key_name) {
  const base::Value::Dict& metrics_dict = prefs->GetDict(prefs::kKioskMetrics);
  const auto* result_value = metrics_dict.Find(key_name);
  if (!result_value) {
    return std::nullopt;
  }
  auto result = result_value->GetIfInt();
  if (!result.has_value()) {
    return std::nullopt;
  }
  return static_cast<ResultType>(result.value());
}

}  // namespace

const char kKioskRamUsagePercentageHistogram[] = "Kiosk.RamUsagePercentage";
const char kKioskSwapUsagePercentageHistogram[] = "Kiosk.SwapUsagePercentage";
const char kKioskDiskUsagePercentageHistogram[] = "Kiosk.DiskUsagePercentage";
const char kKioskChromeProcessCountHistogram[] = "Kiosk.ChromeProcessCount";
const char kKioskSessionRestartInternetAccessHistogram[] =
    "Kiosk.SessionRestart.InternetAccess";
const char kKioskSessionRestartUserActivityHistogram[] =
    "Kiosk.SessionRestart.UserActivity";

const char kKioskInternetAccessInfo[] = "internet-access";
const char kKioskUserActivity[] = "user-activity";

const base::TimeDelta kPeriodicMetricsInterval = base::Hours(1);
const base::TimeDelta kFirstIdleTimeout = base::Minutes(5);
const base::TimeDelta kRegularIdleTimeout = kPeriodicMetricsInterval;

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
    if (!base::PathService::Get(base::DIR_HOME, &path)) {
      NOTREACHED_IN_MIGRATION();
      return;
    }
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

PeriodicMetricsService::PeriodicMetricsService(PrefService* prefs)
    : prefs_(prefs),
      disk_space_calculator_(std::make_unique<DiskSpaceCalculator>()) {}

PeriodicMetricsService::~PeriodicMetricsService() = default;

void PeriodicMetricsService::RecordPreviousSessionMetrics() const {
  RecordPreviousInternetAccessInfo();
  RecordPreviousUserActivity();
}

void PeriodicMetricsService::StartRecordingPeriodicMetrics(
    bool is_offline_enabled) {
  is_offline_enabled_ = is_offline_enabled;
  // Record all periodic metrics at the beginning of the kiosk session and then
  // every `kPeriodicMetricsInterval`.
  RecordPeriodicMetrics(kFirstIdleTimeout);
  metrics_timer_.Start(
      FROM_HERE, kPeriodicMetricsInterval,
      base::BindRepeating(&PeriodicMetricsService::RecordPeriodicMetrics,
                          weak_ptr_factory_.GetWeakPtr(), kRegularIdleTimeout));
}

void PeriodicMetricsService::RecordPeriodicMetrics(
    const base::TimeDelta& idle_timeout) {
  RecordRamUsage();
  RecordSwapUsage();
  RecordDiskSpaceUsage();
  RecordChromeProcessCount();
  SaveInternetAccessInfo();
  SaveUserActivity(idle_timeout);
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
  if (!base::PathService::Get(base::FILE_EXE, &chrome_path)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  base::FilePath::StringType exe_name = chrome_path.BaseName().value();
  int process_count = base::GetProcessCount(exe_name, nullptr);
  base::UmaHistogramCounts100(kKioskChromeProcessCountHistogram, process_count);
}

void PeriodicMetricsService::RecordPreviousInternetAccessInfo() const {
  std::optional<KioskInternetAccessInfo> previous_session_internet_access_info =
      GetSavedEnumFromKioskMetrics<KioskInternetAccessInfo>(
          prefs_, kKioskInternetAccessInfo);

  if (previous_session_internet_access_info.has_value()) {
    base::UmaHistogramEnumeration(
        kKioskSessionRestartInternetAccessHistogram,
        previous_session_internet_access_info.value());
  }
}

void PeriodicMetricsService::RecordPreviousUserActivity() const {
  std::optional<KioskUserActivity> previous_session_user_activity =
      GetSavedEnumFromKioskMetrics<KioskUserActivity>(prefs_,
                                                      kKioskUserActivity);

  if (previous_session_user_activity.has_value()) {
    base::UmaHistogramEnumeration(kKioskSessionRestartUserActivityHistogram,
                                  previous_session_user_activity.value());
  }
}

void PeriodicMetricsService::SaveInternetAccessInfo() const {
  bool is_device_online = IsDeviceOnline();
  KioskInternetAccessInfo network_access_info =
      is_device_online
          ? (is_offline_enabled_
                 ? KioskInternetAccessInfo::kOnlineAndAppSupportsOffline
                 : KioskInternetAccessInfo::kOnlineAndAppRequiresInternet)
          : (is_offline_enabled_
                 ? KioskInternetAccessInfo::kOfflineAndAppSupportsOffline
                 : KioskInternetAccessInfo::kOfflineAndAppRequiresInternet);
  ScopedDictPrefUpdate(prefs_, prefs::kKioskMetrics)
      ->Set(kKioskInternetAccessInfo, static_cast<int>(network_access_info));
}

void PeriodicMetricsService::SaveUserActivity(
    const base::TimeDelta& idle_timeout) const {
  KioskUserActivity user_activity = (GetDeviceIdlePeriod() >= idle_timeout)
                                        ? KioskUserActivity::kIdle
                                        : KioskUserActivity::kActive;

  ScopedDictPrefUpdate(prefs_, prefs::kKioskMetrics)
      ->Set(kKioskUserActivity, static_cast<int>(user_activity));
}

}  // namespace ash
