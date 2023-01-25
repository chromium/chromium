// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/system_info/system_info_card_provider.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/app_list/search/system_info/cpu_data.h"
#include "chrome/browser/ash/app_list/search/system_info/cpu_usage_data.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_util.h"
#include "chrome/common/channel_info.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-shared.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_string.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {
namespace {

using ProbeCategories = ash::cros_healthd::mojom::ProbeCategoryEnum;
using ash::cros_healthd::mojom::BatteryInfo;
using ash::cros_healthd::mojom::CpuInfo;
using ash::cros_healthd::mojom::PhysicalCpuInfoPtr;
using ash::cros_healthd::mojom::TelemetryInfoPtr;
using ::ash::string_matching::FuzzyTokenizedStringMatch;
using ::ash::string_matching::TokenizedString;

constexpr double kRelevanceThreshold = 0.64;

}  // namespace

SystemInfoCardProvider::SystemInfoCardProvider(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  ash::cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
      probe_service_.BindNewPipeAndPassReceiver());
  probe_service_.set_disconnect_handler(
      base::BindOnce(&SystemInfoCardProvider::OnProbeServiceDisconnect,
                     weak_factory_.GetWeakPtr()));
}

SystemInfoCardProvider::~SystemInfoCardProvider() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void SystemInfoCardProvider::Start(const std::u16string& query) {
  // TODO(b/263994165): Replace with complete implementation with keywords
  // stored in translation unit.
  std::vector<std::u16string> memory_keywords = {
      u"memory", u"memory usage", u"ram", u"ram usage", u"activity monitor"};
  for (std::u16string keyword : memory_keywords) {
    if (CalculateRelevance(query, keyword) > kRelevanceThreshold) {
      UpdateMemoryUsage();
      break;
    }
  }

  std::vector<std::u16string> cpu_keywords = {
      u"cpu", u"cpu usage", u"device slow", u"why is my device slow"};
  for (std::u16string keyword : cpu_keywords) {
    if (CalculateRelevance(query, keyword) > kRelevanceThreshold) {
      UpdateCpuUsage();
      break;
    }
  }

  std::vector<std::u16string> battery_keywords = {u"battery", u"battery life",
                                                  u"battery health"};
  for (std::u16string keyword : battery_keywords) {
    if (CalculateRelevance(query, keyword) > kRelevanceThreshold) {
      if (!chromeos::PowerManagerClient::Get()->HasObserver(this)) {
        chromeos::PowerManagerClient::Get()->AddObserver(this);
      }
      UpdateBatteryInfo(absl::nullopt);
      break;
    }
  }

  std::vector<std::u16string> version_keywords = {u"version", u"my device",
                                                  u"about"};
  for (std::u16string keyword : version_keywords) {
    if (CalculateRelevance(query, keyword) > kRelevanceThreshold) {
      UpdateChromeOsVersion();
      break;
    }
  }
}

void SystemInfoCardProvider::StopQuery() {
  // Cancel all previous searches.
  weak_factory_.InvalidateWeakPtrs();
}

double SystemInfoCardProvider::CalculateRelevance(const std::u16string& query,
                                                  const std::u16string& title) {
  const TokenizedString tokenized_title(title, TokenizedString::Mode::kWords);
  const TokenizedString tokenized_query(query,
                                        TokenizedString::Mode::kCamelCase);

  if (tokenized_query.text().empty() || tokenized_title.text().empty()) {
    static constexpr double kDefaultRelevance = 0.0;
    return kDefaultRelevance;
  }

  FuzzyTokenizedStringMatch match;
  return match.Relevance(tokenized_query, tokenized_title,
                         /*use_weighted_ratio=*/false,
                         /*strip_diacritics=*/true,
                         /*use_acronym_matcher=*/true);
}

void SystemInfoCardProvider::BindCrosHealthdProbeServiceIfNecessary() {
  if (!probe_service_ || !probe_service_.is_connected()) {
    ash::cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
        probe_service_.BindNewPipeAndPassReceiver());
    probe_service_.set_disconnect_handler(
        base::BindOnce(&SystemInfoCardProvider::OnProbeServiceDisconnect,
                       weak_factory_.GetWeakPtr()));
  }
}

ash::AppListSearchResultType SystemInfoCardProvider::ResultType() const {
  return ash::AppListSearchResultType::kAnswerCard;
}

void SystemInfoCardProvider::OnProbeServiceDisconnect() {
  probe_service_.reset();
}

void SystemInfoCardProvider::OnMemoryUsageUpdated(TelemetryInfoPtr info_ptr) {
  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    return;
  }

  memory_info_ = GetMemoryInfo(*info_ptr);
}

void SystemInfoCardProvider::UpdateMemoryUsage() {
  BindCrosHealthdProbeServiceIfNecessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kMemory},
      base::BindOnce(&SystemInfoCardProvider::OnMemoryUsageUpdated,
                     weak_factory_.GetWeakPtr()));
}

void SystemInfoCardProvider::OnCpuUsageUpdated(TelemetryInfoPtr info_ptr) {
  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    return;
  }

  const CpuInfo* cpu_info = GetCpuInfo(*info_ptr);
  if (cpu_info == nullptr) {
    LOG(ERROR) << "No CpuInfo in response from cros_healthd.";
    return;
  }

  if (cpu_info->physical_cpus.empty()) {
    LOG(ERROR) << "Device reported having zero physical CPUs.";
    return;
  }

  if (cpu_info->physical_cpus[0]->logical_cpus.empty()) {
    LOG(ERROR) << "Device reported having zero logical CPUs.";
    return;
  }

  // For simplicity, assume that all devices have just one physical CPU, made
  // up of one or more virtual CPUs.
  if (cpu_info->physical_cpus.size() > 1) {
    VLOG(1) << "Device has more than one physical CPU.";
  }

  const PhysicalCpuInfoPtr& physical_cpu_ptr = cpu_info->physical_cpus[0];

  CpuUsageData new_cpu_usage_data =
      CalculateCpuUsage(physical_cpu_ptr->logical_cpus);
  std::unique_ptr<CpuData> new_cpu_usage = std::make_unique<CpuData>();

  PopulateCpuUsage(new_cpu_usage_data, previous_cpu_usage_data_,
                   *new_cpu_usage.get());
  PopulateAverageCpuTemperature(*cpu_info, *new_cpu_usage.get());
  PopulateAverageScaledClockSpeed(*cpu_info, *new_cpu_usage.get());

  previous_cpu_usage_data_ = new_cpu_usage_data;
  cpu_usage_ = std::move(new_cpu_usage);
}

void SystemInfoCardProvider::UpdateCpuUsage() {
  BindCrosHealthdProbeServiceIfNecessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kCpu},
      base::BindOnce(&SystemInfoCardProvider::OnCpuUsageUpdated,
                     weak_factory_.GetWeakPtr()));
}

void SystemInfoCardProvider::UpdateBatteryInfo(
    absl::optional<power_manager::PowerSupplyProperties>
        power_supply_properties) {
  BindCrosHealthdProbeServiceIfNecessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBattery},
      base::BindOnce(&SystemInfoCardProvider::OnBatteryInfoUpdated,
                     weak_factory_.GetWeakPtr(), power_supply_properties));
}

void SystemInfoCardProvider::OnBatteryInfoUpdated(
    absl::optional<power_manager::PowerSupplyProperties>
        power_supply_properties,
    ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr) {
  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    return;
  }

  const BatteryInfo* battery_info_ptr = GetBatteryInfo(*info_ptr);
  if (!battery_info_ptr) {
    LOG(ERROR) << "BatteryInfo requested by device does not have a battery.";
    return;
  }

  std::unique_ptr<BatteryHealth> new_battery_health =
      std::make_unique<BatteryHealth>();

  PopulateBatteryHealth(*battery_info_ptr, *new_battery_health.get());

  const absl::optional<power_manager::PowerSupplyProperties>& proto =
      power_supply_properties.has_value()
          ? power_supply_properties
          : chromeos::PowerManagerClient::Get()->GetLastStatus();
  DCHECK(proto);

  PopulatePowerStatus(proto.value(), *new_battery_health.get());

  battery_health_ = std::move(new_battery_health);
}

void SystemInfoCardProvider::PowerChanged(
    const power_manager::PowerSupplyProperties& power_supply_properties) {
  UpdateBatteryInfo(absl::make_optional<power_manager::PowerSupplyProperties>(
      power_supply_properties));
}

void SystemInfoCardProvider::UpdateChromeOsVersion() {
  std::string version = version_info::GetVersionStringWithModifier("");
  std::string official = l10n_util::GetStringUTF8(
      version_info::IsOfficialBuild() ? IDS_VERSION_UI_OFFICIAL
                                      : IDS_VERSION_UI_UNOFFICIAL);
  std::string processorVariation = l10n_util::GetStringUTF8(
      sizeof(void*) == 8 ? IDS_VERSION_UI_64BIT : IDS_VERSION_UI_32BIT);

  // TODO(b/263994165): Replace this with the correct translation string.
  chromeOS_version_ =
      std::string("Version " + version + " (" + official + ") " +
                  chrome::GetChannelName(chrome::WithExtendedStable(true)) +
                  " " + processorVariation);
}

}  // namespace app_list
