// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_CARD_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_CARD_PROVIDER_H_

#include <bitset>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/calculator/size_calculator.h"
#include "chromeos/ash/components/launcher_search/system_info/system_info_keyword_input.h"
#include "chromeos/ash/components/system_info/battery_health.h"
#include "chromeos/ash/components/system_info/cpu_usage_data.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {

/* This Provider intents to return answer cards which surface system-level
information such as Storage usage, CPU consumption, battery health, current
version, network information and memory usage. The answer cards link to the
relevant pages within the Settings and Diagnostics apps.*/

// TODO(b/263994165): Complete the System Info Card Provider to return results.
// This provider is a work in progress.
class SystemInfoCardProvider : public SearchProvider,
                               public ash::settings::SizeCalculator::Observer {
 public:
  using UpdateCpuResultCallback = base::RepeatingCallback<void(bool)>;
  using UpdateMemoryResultCallback = base::RepeatingCallback<void(bool)>;

  // Implemented by clients that wish to be updated periodically about the
  // cpu usage of the device.
  class CpuDataObserver : public base::CheckedObserver {
   public:
    virtual void OnCpuDataUpdated(
        const std::u16string& title,
        const std::u16string& description,
        const std::u16string& accessibility_label) = 0;
  };

  // Implemented by clients that wish to be updated periodically about the
  // cpu usage of the device.
  class MemoryObserver : public base::CheckedObserver {
   public:
    virtual void OnMemoryUpdated(const double memory_usage_percentage,
                                 const std::u16string& description,
                                 const std::u16string& accessibility_label) = 0;
  };

  explicit SystemInfoCardProvider(Profile* profile);
  ~SystemInfoCardProvider() override;

  SystemInfoCardProvider(const SystemInfoCardProvider&) = delete;
  SystemInfoCardProvider& operator=(const SystemInfoCardProvider&) = delete;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StopQuery() override;
  ash::AppListSearchResultType ResultType() const override;

  // SizeCalculator::Observer:
  void OnSizeCalculated(
      const ash::settings::SizeCalculator::CalculationType& calculation_type,
      int64_t total_bytes) override;

  // Adds and removes the Cpu Data observer.
  virtual void AddCpuDataObserver(CpuDataObserver* observer);
  virtual void RemoveCpuDataObserver(CpuDataObserver* observer);

  // Adds and removes the Memory observer.
  virtual void AddMemoryObserver(MemoryObserver* observer);
  virtual void RemoveMemoryObserver(MemoryObserver* observer);

  void SetCpuUsageTimerForTesting(std::unique_ptr<base::RepeatingTimer> timer);
  void SetMemoryTimerForTesting(std::unique_ptr<base::RepeatingTimer> timer);

 private:
  void BindCrosHealthdProbeServiceIfNecessary();
  void OnProbeServiceDisconnect();
  double CalculateRelevance(const std::u16string& query,
                            const std::u16string& title);

  void UpdateMemoryUsage(bool create_result);
  void OnMemoryUsageUpdated(
      bool create_result,
      ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void UpdateCpuUsage(bool create_result);
  void OnCpuUsageUpdated(bool create_result,
                         ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void UpdateBatteryInfo();
  void OnBatteryInfoUpdated(
      ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void UpdateChromeOsVersion();

  void UpdateStorageInfo();
  void StartObservingCalculators();
  void OnStorageInfoUpdated();
  void StopObservingCalculators();
  void CreateStorageAnswerCard();

  // Instances calculating the size of each storage items.
  ::ash::settings::TotalDiskSpaceCalculator total_disk_space_calculator_;
  ::ash::settings::FreeDiskSpaceCalculator free_disk_space_calculator_;
  ::ash::settings::MyFilesSizeCalculator my_files_size_calculator_;
  ::ash::settings::DriveOfflineSizeCalculator drive_offline_size_calculator_;
  ::ash::settings::BrowsingDataSizeCalculator browsing_data_size_calculator_;
  ::ash::settings::CrostiniSizeCalculator crostini_size_calculator_;
  ::ash::settings::OtherUsersSizeCalculator other_users_size_calculator_;

  // Keeps track of the size of each storage item. Adding 1 since we are also
  // saving the system storage here
  int64_t storage_items_total_bytes_
      [::ash::settings::SizeCalculator::kCalculationTypeCount + 1] = {0};

  // Controls if the size of each storage item has been calculated.
  std::bitset<::ash::settings::SizeCalculator::kCalculationTypeCount>
      calculation_state_;

  // Last query. It is reset when view is closed.
  std::u16string last_query_;

  const raw_ptr<Profile> profile_;
  double relevance_;
  mojo::Remote<ash::cros_healthd::mojom::CrosHealthdProbeService>
      probe_service_;
  std::string chromeOS_version_{""};
  system_info::CpuUsageData previous_cpu_usage_data_{
      system_info::CpuUsageData()};
  raw_ptr<ash::cros_healthd::mojom::MemoryInfo, DanglingUntriaged> memory_info_{
      nullptr};
  std::unique_ptr<system_info::BatteryHealth> battery_health_{nullptr};
  gfx::ImageSkia os_settings_icon_;
  gfx::ImageSkia diagnostics_icon_;
  std::vector<launcher_search::SystemInfoKeywordInput> keywords_;
  std::unique_ptr<base::RepeatingTimer> cpu_usage_timer_;
  std::unique_ptr<base::RepeatingTimer> memory_timer_;
  base::ObserverList<SystemInfoCardProvider::CpuDataObserver> cpu_observers_;
  base::ObserverList<SystemInfoCardProvider::MemoryObserver> memory_observers_;

  base::WeakPtrFactory<SystemInfoCardProvider> weak_factory_{this};
};
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_CARD_PROVIDER_H_
