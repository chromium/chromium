// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_CARD_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_CARD_PROVIDER_H_

#include <bitset>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/system_info/battery_health.h"
#include "chrome/browser/ash/app_list/search/system_info/cpu_data.h"
#include "chrome/browser/ash/app_list/search/system_info/cpu_usage_data.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_keyword_input.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/ash/calculator/size_calculator.h"
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

 private:
  void BindCrosHealthdProbeServiceIfNecessary();
  void OnProbeServiceDisconnect();
  double CalculateRelevance(const std::u16string& query,
                            const std::u16string& title);

  void UpdateMemoryUsage();
  void OnMemoryUsageUpdated(
      ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void UpdateCpuUsage();
  void OnCpuUsageUpdated(ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr);

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
  ::ash::settings::AppsSizeCalculator apps_size_calculator_;
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

  Profile* const profile_;
  double relevance_;
  mojo::Remote<ash::cros_healthd::mojom::CrosHealthdProbeService>
      probe_service_;
  std::string chromeOS_version_{""};
  CpuUsageData previous_cpu_usage_data_{CpuUsageData()};
  ash::cros_healthd::mojom::MemoryInfo* memory_info_{nullptr};
  std::unique_ptr<CpuData> cpu_usage_{nullptr};
  std::unique_ptr<BatteryHealth> battery_health_{nullptr};
  gfx::ImageSkia os_settings_icon_;
  gfx::ImageSkia diagnostics_icon_;
  std::vector<SystemInfoKeywordInput> keywords_;

  base::WeakPtrFactory<SystemInfoCardProvider> weak_factory_{this};
};
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_CARD_PROVIDER_H_
