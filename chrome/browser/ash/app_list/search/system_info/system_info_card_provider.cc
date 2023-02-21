// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/system_info/system_info_card_provider.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/system_info/cpu_data.h"
#include "chrome/browser/ash/app_list/search/system_info/cpu_usage_data.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_answer_result.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_util.h"
#include "chrome/browser/ash/app_list/vector_icons/vector_icons.h"
#include "chrome/browser/ui/webui/settings/ash/device_storage_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom-forward.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-shared.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_string.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

namespace app_list {
namespace {

using SizeCalculator = ::ash::settings::SizeCalculator;
using ProbeCategories = ::ash::cros_healthd::mojom::ProbeCategoryEnum;
using ::ash::cros_healthd::mojom::BatteryInfo;
using ::ash::cros_healthd::mojom::CpuInfo;
using ::ash::cros_healthd::mojom::PhysicalCpuInfoPtr;
using ::ash::cros_healthd::mojom::TelemetryInfoPtr;
using ::ash::string_matching::FuzzyTokenizedStringMatch;
using ::ash::string_matching::TokenizedString;
using ::chromeos::settings::mojom::kAboutChromeOsSectionPath;

constexpr double kRelevanceThreshold = 0.64;

}  // namespace

SystemInfoCardProvider::SystemInfoCardProvider(Profile* profile)
    : total_disk_space_calculator_(profile),
      free_disk_space_calculator_(profile),
      my_files_size_calculator_(profile),
      browsing_data_size_calculator_(profile),
      apps_size_calculator_(profile),
      crostini_size_calculator_(profile),
      profile_(profile) {
  DCHECK(profile_);
  ash::cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
      probe_service_.BindNewPipeAndPassReceiver());
  probe_service_.set_disconnect_handler(
      base::BindOnce(&SystemInfoCardProvider::OnProbeServiceDisconnect,
                     weak_factory_.GetWeakPtr()));
  StartObservingCalculators();
  chromeos::PowerManagerClient::Get()->AddObserver(this);

  // TODO(b/261867385): We manually load the icon from the local codebase as
  // the icon load from proxy is flaky. When the flakiness if solved, we can
  // safely remove this and add the logic to load icons from proxy.
  os_settings_icon_ = gfx::CreateVectorIcon(
      app_list::kOsSettingsIcon, kAppIconDimension, SK_ColorTRANSPARENT);
  diagnostics_icon_ = gfx::CreateVectorIcon(
      app_list::kDiagnosticsIcon, kAppIconDimension, SK_ColorTRANSPARENT);
}

SystemInfoCardProvider::~SystemInfoCardProvider() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  StopObservingCalculators();
}

void SystemInfoCardProvider::Start(const std::u16string& query) {
  // TODO(b/263994165): Replace with complete implementation with keywords
  // stored in translation unit.
  std::vector<std::u16string> memory_keywords = {
      u"memory", u"memory usage", u"ram", u"ram usage", u"activity monitor"};
  for (const std::u16string& keyword : memory_keywords) {
    double relevance = CalculateRelevance(query, keyword);
    if (relevance > kRelevanceThreshold) {
      relevance_ = relevance;
      UpdateMemoryUsage();
      break;
    }
  }

  std::vector<std::u16string> cpu_keywords = {
      u"cpu", u"cpu usage", u"device slow", u"why is my device slow"};
  for (const std::u16string& keyword : cpu_keywords) {
    double relevance = CalculateRelevance(query, keyword);
    if (relevance > kRelevanceThreshold) {
      relevance_ = relevance;
      UpdateCpuUsage();
      break;
    }
  }

  std::vector<std::u16string> battery_keywords = {u"battery", u"battery life",
                                                  u"battery health"};
  for (const std::u16string& keyword : battery_keywords) {
    double relevance = CalculateRelevance(query, keyword);
    if (relevance > kRelevanceThreshold) {
      relevance_ = relevance;
      UpdateBatteryInfo(absl::nullopt);
      break;
    }
  }

  std::vector<std::u16string> version_keywords = {u"version", u"my device",
                                                  u"about"};
  for (const std::u16string& keyword : version_keywords) {
    double relevance = CalculateRelevance(query, keyword);
    if (relevance > kRelevanceThreshold) {
      relevance_ = relevance;
      UpdateChromeOsVersion();
      break;
    }
  }

  std::vector<std::u16string> storage_keywords = {u"storage", u"storage use",
                                                  u"storage management"};
  for (const std::u16string& keyword : storage_keywords) {
    double relevance = CalculateRelevance(query, keyword);
    if (relevance > kRelevanceThreshold) {
      // Do not calculate the storage size again if already calculated
      // recently.
      // TODO(b/263994165): Add in a refresh period here.
      relevance_ = relevance;
      if (!calculation_state_.all()) {
        UpdateStorageInfo();
      }
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
  return ash::AppListSearchResultType::kSystemInfo;
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
  std::u16string title = l10n_util::GetStringFUTF16(
      IDS_ASH_CPU_IN_LAUNCHER_TITLE, cpu_usage_->GetPercentUsageTotalString());
  std::u16string description = l10n_util::GetStringFUTF16(
      IDS_ASH_CPU_IN_LAUNCHER_DESCRIPTION,
      base::NumberToString16(cpu_usage_->GetAverageCpuTempCelsius()),
      // Provide the frequency in GHz and round the value to 2 decimal places.
      base::NumberToString16(
          static_cast<double>(
              cpu_usage_->GetScalingAverageCurrentFrequencyKhz() / 10000) /
          100));
  SearchProvider::Results new_results;
  new_results.emplace_back(std::make_unique<SystemInfoAnswerResult>(
      profile_, last_query_, /*url_path=*/"", diagnostics_icon_, relevance_,
      title, description,
      SystemInfoAnswerResult::AnswerCardDisplayType::kTextCard,
      SystemInfoAnswerResult::SystemInfoCategory::kDiagnostics));
  SwapResults(&new_results);
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
  std::u16string version =
      base::UTF8ToUTF16(version_info::GetVersionStringWithModifier(""));
  std::u16string is_official = l10n_util::GetStringUTF16(
      version_info::IsOfficialBuild() ? IDS_VERSION_UI_OFFICIAL
                                      : IDS_VERSION_UI_UNOFFICIAL);
  std::u16string processor_variation = l10n_util::GetStringUTF16(
      sizeof(void*) == 8 ? IDS_VERSION_UI_64BIT : IDS_VERSION_UI_32BIT);

  std::u16string version_string = l10n_util::GetStringFUTF16(
      IDS_ASH_VERSION_IN_LAUNCHER_MESSAGE, version, is_official,
      base::UTF8ToUTF16(
          chrome::GetChannelName(chrome::WithExtendedStable(true))),
      processor_variation);
  std::u16string description =
      l10n_util::GetStringUTF16(IDS_SETTINGS_ABOUT_PAGE_CHECK_FOR_UPDATES);
  SearchProvider::Results new_results;
  new_results.emplace_back(std::make_unique<SystemInfoAnswerResult>(
      profile_, last_query_, kAboutChromeOsSectionPath, os_settings_icon_,
      relevance_, version_string, description,
      SystemInfoAnswerResult::AnswerCardDisplayType::kTextCard,
      SystemInfoAnswerResult::SystemInfoCategory::kSettings));
  SwapResults(&new_results);
}

void SystemInfoCardProvider::UpdateStorageInfo() {
  total_disk_space_calculator_.StartCalculation();
  free_disk_space_calculator_.StartCalculation();
  my_files_size_calculator_.StartCalculation();
  browsing_data_size_calculator_.StartCalculation();
  apps_size_calculator_.StartCalculation();
  crostini_size_calculator_.StartCalculation();
  other_users_size_calculator_.StartCalculation();
}

void SystemInfoCardProvider::StartObservingCalculators() {
  total_disk_space_calculator_.AddObserver(this);
  free_disk_space_calculator_.AddObserver(this);
  my_files_size_calculator_.AddObserver(this);
  browsing_data_size_calculator_.AddObserver(this);
  apps_size_calculator_.AddObserver(this);
  crostini_size_calculator_.AddObserver(this);
  other_users_size_calculator_.AddObserver(this);
}

void SystemInfoCardProvider::StopObservingCalculators() {
  total_disk_space_calculator_.RemoveObserver(this);
  free_disk_space_calculator_.RemoveObserver(this);
  my_files_size_calculator_.RemoveObserver(this);
  browsing_data_size_calculator_.RemoveObserver(this);
  apps_size_calculator_.RemoveObserver(this);
  crostini_size_calculator_.RemoveObserver(this);
  other_users_size_calculator_.RemoveObserver(this);
}

void SystemInfoCardProvider::OnSizeCalculated(
    const SizeCalculator::CalculationType& calculation_type,
    int64_t total_bytes) {
  // The total disk space is rounded to the next power of 2.
  if (calculation_type == SizeCalculator::CalculationType::kTotal) {
    total_bytes = ash::settings::RoundByteSize(total_bytes);
  }

  // Store calculated item's size.
  const int item_index = static_cast<int>(calculation_type);
  storage_items_total_bytes_[item_index] = total_bytes;

  // Mark item as calculated.
  calculation_state_.set(item_index);
  OnStorageInfoUpdated();
}

void SystemInfoCardProvider::OnStorageInfoUpdated() {
  // If some size calculations are pending, return early and wait for all
  // calculations to complete.
  if (!calculation_state_.all()) {
    return;
  }

  const int total_space_index =
      static_cast<int>(SizeCalculator::CalculationType::kTotal);
  const int free_disk_space_index =
      static_cast<int>(SizeCalculator::CalculationType::kAvailable);

  int64_t total_bytes = storage_items_total_bytes_[total_space_index];
  int64_t available_bytes = storage_items_total_bytes_[free_disk_space_index];
  int64_t in_use_bytes = total_bytes - available_bytes;

  if (total_bytes <= 0 || available_bytes < 0) {
    // We can't get useful information from the storage page if total_bytes <=
    // 0 or available_bytes is less than 0. This is not expected to happen.
    NOTREACHED() << "Unable to retrieve total or available disk space";
    return;
  }

  int64_t system_bytes = 0;
  for (int i = 0; i < SizeCalculator::kCalculationTypeCount; ++i) {
    const int64_t total_bytes_for_current_item =
        std::max(storage_items_total_bytes_[i], static_cast<int64_t>(0));
    // The total amount of disk space counts positively towards system's size.
    if (i == static_cast<int>(SizeCalculator::CalculationType::kTotal)) {
      if (total_bytes_for_current_item <= 0) {
        return;
      }
      system_bytes += total_bytes_for_current_item;
      continue;
    }
    // All other items are subtracted from the total amount of disk space.
    if (i == static_cast<int>(SizeCalculator::CalculationType::kAvailable) &&
        total_bytes_for_current_item < 0) {
      return;
    }
    system_bytes -= total_bytes_for_current_item;
  }
  const int system_space_index =
      static_cast<int>(SizeCalculator::CalculationType::kSystem);
  storage_items_total_bytes_[system_space_index] = system_bytes;
  std::u16string in_use_size = ui::FormatBytes(in_use_bytes);
  std::u16string total_size = ui::FormatBytes(total_bytes);
  // TODO(b/263994165): Add this string into an answer result.
  std::u16string title = base::StrCat({in_use_size, u" in use / ", total_size});
}

}  // namespace app_list
