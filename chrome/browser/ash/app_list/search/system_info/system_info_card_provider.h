// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_CARD_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_CARD_PROVIDER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/system_info/battery_health.h"
#include "chrome/browser/ash/app_list/search/system_info/cpu_data.h"
#include "chrome/browser/ash/app_list/search/system_info/cpu_usage_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace app_list {

/* This Provider intents to return answer cards which surface system-level
information such as Storage usage, CPU consumption, battery health, current
version, network information and memory usage. The answer cards link to the
relevant pages within the Settings and Diagnostics apps.*/

// TODO(b/263994165): Complete the System Info Card Provide to return results.
// This provider is a work in progress.
class SystemInfoCardProvider : public SearchProvider,
                               public chromeos::PowerManagerClient::Observer {
 public:
  explicit SystemInfoCardProvider(Profile* profile);
  ~SystemInfoCardProvider() override;

  SystemInfoCardProvider(const SystemInfoCardProvider&) = delete;
  SystemInfoCardProvider& operator=(const SystemInfoCardProvider&) = delete;
  ash::AppListSearchResultType ResultType() const override;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StopQuery() override;

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

  void UpdateBatteryInfo(absl::optional<power_manager::PowerSupplyProperties>
                             power_supply_properties);
  void OnBatteryInfoUpdated(
      absl::optional<power_manager::PowerSupplyProperties>
          power_supply_properties,
      ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  // chromeos::PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties&
                        power_supply_properties) override;

  void UpdateChromeOsVersion();

  Profile* const profile_;
  mojo::Remote<ash::cros_healthd::mojom::CrosHealthdProbeService>
      probe_service_;
  std::string chromeOS_version_{""};
  CpuUsageData previous_cpu_usage_data_{CpuUsageData()};
  ash::cros_healthd::mojom::MemoryInfo* memory_info_{nullptr};
  std::unique_ptr<CpuData> cpu_usage_{nullptr};
  std::unique_ptr<BatteryHealth> battery_health_{nullptr};
  base::WeakPtrFactory<SystemInfoCardProvider> weak_factory_{this};
};
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_CARD_PROVIDER_H_
