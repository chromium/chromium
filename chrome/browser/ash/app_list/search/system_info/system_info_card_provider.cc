// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/app_list/search/system_info/system_info_card_provider.h"

#include <iomanip>
#include <memory>
#include <optional>
#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/system_info/battery_answer_result.h"
#include "chrome/browser/ash/app_list/search/system_info/cpu_answer_result.h"
#include "chrome/browser/ash/app_list/search/system_info/memory_answer_result.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_answer_result.h"
#include "chrome/browser/ash/app_list/vector_icons/vector_icons.h"
#include "chrome/browser/ui/webui/ash/settings/calculator/size_calculator.h"
#include "chrome/browser/ui/webui/ash/settings/pages/storage/device_storage_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/launcher_search/system_info/launcher_util.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/ash/components/system_info/cpu_data.h"
#include "chromeos/ash/components/system_info/cpu_usage_data.h"
#include "chromeos/ash/components/system_info/system_info_util.h"
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
using ::chromeos::settings::mojom::kStorageSubpagePath;
using AnswerCardInfo = ::ash::SystemInfoAnswerCardData;

constexpr double kMinimumRelevance = 0.0;
constexpr double kRelevanceThreshold = 0.79;
constexpr double kMinimumQueryLength = 3;

constexpr char kHistogramMemoryCrosHealthdProbeErrorPrefix[] =
    "Apps.AppList.SystemInfoProvider.CrosHealthdProbeError.MemoryInfo";
constexpr char kHistogramCpuCrosHealthdProbeErrorPrefix[] =
    "Apps.AppList.SystemInfoProvider.CrosHealthdProbeError.CpuInfo";
constexpr char kHistogramBatteryCrosHealthdProbeErrorPrefix[] =
    "Apps.AppList.SystemInfoProvider.CrosHealthdProbeError.BatteryInfo";
constexpr char kHistogramBatteryErrorPrefix[] =
    "Apps.AppList.SystemInfoProvider.Error.Battery";

double ConvertKBtoBytes(uint32_t amount) {
  return static_cast<double>(amount) * 1024;
}

}  // namespace

SystemInfoCardProvider::SystemInfoCardProvider(Profile* profile)
    : SearchProvider(SearchCategory::kSystemInfoCard),
      total_disk_space_calculator_(profile),
      free_disk_space_calculator_(profile),
      my_files_size_calculator_(profile),
      drive_offline_size_calculator_(profile),
      browsing_data_size_calculator_(profile),
      crostini_size_calculator_(profile),
      profile_(profile),
      keywords_(launcher_search::GetSystemInfoKeywordVector()) {
  DCHECK(profile_);
  ash::cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
      probe_service_.BindNewPipeAndPassReceiver());
  probe_service_.set_disconnect_handler(
      base::BindOnce(&SystemInfoCardProvider::OnProbeServiceDisconnect,
                     weak_factory_.GetWeakPtr()));
  StartObservingCalculators();
  cpu_usage_timer_ = std::make_unique<base::RepeatingTimer>();
  memory_timer_ = std::make_unique<base::RepeatingTimer>();

  // TODO(b/261867385): We manually load the icon from the local codebase as
  // the icon load from proxy is flaky. When the flakiness if solved, we can
  // safely remove this and add the logic to load icons from proxy.
  os_settings_icon_ = gfx::CreateVectorIcon(
      app_list::kOsSettingsIcon, kAppIconDimension, SK_ColorTRANSPARENT);
  diagnostics_icon_ = gfx::CreateVectorIcon(
      app_list::kDiagnosticsIcon, kAppIconDimension, SK_ColorTRANSPARENT);
}

SystemInfoCardProvider::~SystemInfoCardProvider() {
  StopObservingCalculators();
}

void SystemInfoCardProvider::Start(const std::u16string& query) {
  if (query.length() < kMinimumQueryLength) {
    return;
  }

  double max_relevance = 0;
  launcher_search::SystemInfoKeywordInput* most_relevant_keyword_input;
  for (launcher_search::SystemInfoKeywordInput& keyword_input : keywords_) {
    double relevance = CalculateRelevance(query, keyword_input.GetKeyword());
    if (relevance > kRelevanceThreshold && relevance > max_relevance) {
      max_relevance = relevance;
      most_relevant_keyword_input = &keyword_input;
    }
  }

  if (max_relevance > kRelevanceThreshold) {
    relevance_ = max_relevance;
    switch (most_relevant_keyword_input->GetInputType()) {
      case launcher_search::SystemInfoInputType::kMemory:
        UpdateMemoryUsage(/*create_result=*/true);
        break;
      case launcher_search::SystemInfoInputType::kCPU:
        UpdateCpuUsage(/*create_result=*/true);
        break;
      case launcher_search::SystemInfoInputType::kVersion:
        UpdateChromeOsVersion();
        break;
      // Do not calculate the storage size again if already
      // calculated recently.
      // TODO(b/263994165): Add in a refresh period here.
      case launcher_search::SystemInfoInputType::kStorage:
        if (!calculation_state_.all()) {
          UpdateStorageInfo();
        } else {
          CreateStorageAnswerCard();
        }
        break;
      case launcher_search::SystemInfoInputType::kBattery:
        UpdateBatteryInfo();
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
  const TokenizedString tokenized_query(query, TokenizedString::Mode::kWords);

  if (tokenized_query.text().empty() || tokenized_title.text().empty()) {
    return kMinimumRelevance;
  }

  return FuzzyTokenizedStringMatch::TokenSortRatio(
      tokenized_query, tokenized_title, /*partial=*/false);
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

void SystemInfoCardProvider::OnMemoryUsageUpdated(bool create_result,
                                                  TelemetryInfoPtr info_ptr) {
  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    return;
  }

  memory_info_ = system_info::GetMemoryInfo(
      *info_ptr, kHistogramMemoryCrosHealthdProbeErrorPrefix);
  if (!memory_info_) {
    LOG(ERROR) << "Memory information not provided by croshealthd";
    return;
  }

  std::u16string available_memory_gb =
      ui::FormatBytes(ConvertKBtoBytes(memory_info_->available_memory_kib));
  std::u16string total_memory_gb =
      ui::FormatBytes(ConvertKBtoBytes(memory_info_->total_memory_kib));

  double used_memory_kb =
      memory_info_->total_memory_kib - memory_info_->available_memory_kib;
  double memory_usage_percentage =
      static_cast<double>(used_memory_kb) * 100 /
      static_cast<double>(memory_info_->total_memory_kib);

  std::u16string description =
      l10n_util::GetStringFUTF16(IDS_ASH_MEMORY_USAGE_IN_LAUNCHER_DESCRIPTION,
                                 available_memory_gb, total_memory_gb);

  std::u16string accessibility_label_details = l10n_util::GetStringFUTF16(
      IDS_ASH_MEMORY_USAGE_IN_LAUNCHER_ACCESSIBILITY_LABEL, available_memory_gb,
      total_memory_gb);

  if (create_result) {
    AnswerCardInfo answer_card_info(memory_usage_percentage);
    // The bar chart will turn red if there is less than 10% of memory free.
    answer_card_info.SetUpperLimitForBarChart(90);
    SearchProvider::Results new_results;
    DCHECK(memory_timer_);
    new_results.emplace_back(std::make_unique<MemoryAnswerResult>(
        profile_, last_query_, /*url_path=*/std::string(), diagnostics_icon_,
        relevance_,
        /*title=*/std::u16string(), description, accessibility_label_details,
        SystemInfoAnswerResult::SystemInfoCategory::kDiagnostics,
        SystemInfoAnswerResult::SystemInfoCardType::kMemory, answer_card_info,
        base::BindRepeating(&SystemInfoCardProvider::UpdateMemoryUsage,
                            weak_factory_.GetWeakPtr()),
        std::move(memory_timer_), this));
    SwapResults(&new_results);
    memory_timer_ = std::make_unique<base::RepeatingTimer>();
  } else {
    for (auto& observer : memory_observers_) {
      observer.OnMemoryUpdated(memory_usage_percentage, description,
                               accessibility_label_details);
    }
  }
}

void SystemInfoCardProvider::UpdateMemoryUsage(bool create_result) {
  BindCrosHealthdProbeServiceIfNecessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kMemory},
      base::BindOnce(&SystemInfoCardProvider::OnMemoryUsageUpdated,
                     weak_factory_.GetWeakPtr(), create_result));
}

void SystemInfoCardProvider::OnCpuUsageUpdated(bool create_result,
                                               TelemetryInfoPtr info_ptr) {
  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    return;
  }

  const CpuInfo* cpu_info = system_info::GetCpuInfo(
      *info_ptr, kHistogramCpuCrosHealthdProbeErrorPrefix);
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

  system_info::CpuUsageData new_cpu_usage_data =
      system_info::CalculateCpuUsage(physical_cpu_ptr->logical_cpus);
  std::unique_ptr<system_info::CpuData> new_cpu_usage =
      std::make_unique<system_info::CpuData>();

  system_info::PopulateCpuUsage(new_cpu_usage_data, previous_cpu_usage_data_,
                                *new_cpu_usage.get());
  system_info::PopulateAverageCpuTemperature(*cpu_info, *new_cpu_usage.get());
  system_info::PopulateAverageScaledClockSpeed(*cpu_info, *new_cpu_usage.get());

  previous_cpu_usage_data_ = new_cpu_usage_data;
  std::u16string cpu_temp =
      base::NumberToString16(new_cpu_usage->GetAverageCpuTempCelsius());
  // Provide the frequency in GHz and round the value to 2 decimal places.
  std::u16string cpu_speed = base::NumberToString16(
      static_cast<double>(
          new_cpu_usage->GetScalingAverageCurrentFrequencyKhz() / 10000) /
      100);
  std::u16string title =
      l10n_util::GetStringFUTF16(IDS_ASH_CPU_IN_LAUNCHER_TITLE,
                                 new_cpu_usage->GetPercentUsageTotalString());
  std::u16string description = l10n_util::GetStringFUTF16(
      IDS_ASH_CPU_IN_LAUNCHER_DESCRIPTION, cpu_temp, cpu_speed);
  std::u16string accessibility_label_details = l10n_util::GetStringFUTF16(
      IDS_ASH_CPU_IN_LAUNCHER_ACCESSIBILITY_LABEL,
      new_cpu_usage->GetPercentUsageTotalString(), cpu_temp, cpu_speed);

  if (create_result) {
    AnswerCardInfo answer_card_info(
        ash::SystemInfoAnswerCardDisplayType::kTextCard);
    SearchProvider::Results new_results;
    DCHECK(cpu_usage_timer_);
    new_results.emplace_back(std::make_unique<CpuAnswerResult>(
        profile_, last_query_, /*url_path=*/std::string(), diagnostics_icon_,
        relevance_, title, description, accessibility_label_details,
        SystemInfoAnswerResult::SystemInfoCategory::kDiagnostics,
        SystemInfoAnswerResult::SystemInfoCardType::kCPU, answer_card_info,
        base::BindRepeating(&SystemInfoCardProvider::UpdateCpuUsage,
                            weak_factory_.GetWeakPtr()),
        std::move(cpu_usage_timer_), this));
    SwapResults(&new_results);
    cpu_usage_timer_ = std::make_unique<base::RepeatingTimer>();
  } else {
    for (auto& observer : cpu_observers_) {
      observer.OnCpuDataUpdated(title, description,
                                accessibility_label_details);
    }
  }
}

void SystemInfoCardProvider::UpdateCpuUsage(bool create_result) {
  BindCrosHealthdProbeServiceIfNecessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kCpu},
      base::BindOnce(&SystemInfoCardProvider::OnCpuUsageUpdated,
                     weak_factory_.GetWeakPtr(), create_result));
}

void SystemInfoCardProvider::UpdateBatteryInfo() {
  BindCrosHealthdProbeServiceIfNecessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBattery},
      base::BindOnce(&SystemInfoCardProvider::OnBatteryInfoUpdated,
                     weak_factory_.GetWeakPtr()));
}

void SystemInfoCardProvider::OnBatteryInfoUpdated(
    ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr) {
  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    return;
  }

  const BatteryInfo* battery_info_ptr = system_info::GetBatteryInfo(
      *info_ptr, kHistogramBatteryCrosHealthdProbeErrorPrefix,
      kHistogramBatteryErrorPrefix);
  if (!battery_info_ptr) {
    LOG(ERROR) << "BatteryInfo requested by device does not have a battery.";
    return;
  }

  std::unique_ptr<system_info::BatteryHealth> new_battery_health =
      std::make_unique<system_info::BatteryHealth>();

  system_info::PopulateBatteryHealth(*battery_info_ptr,
                                     *new_battery_health.get());

  const std::optional<power_manager::PowerSupplyProperties>& proto =
      chromeos::PowerManagerClient::Get()->GetLastStatus();
  if (!proto) {
    system_info::EmitBatteryDataError(system_info::BatteryDataError::kNoData,
                                      kHistogramBatteryErrorPrefix);
    return;
  }

  launcher_search::PopulatePowerStatus(proto.value(),
                                       *new_battery_health.get());

  std::u16string battery_health_info = l10n_util::GetStringFUTF16(
      IDS_ASH_BATTERY_STATUS_IN_LAUNCHER_DESCRIPTION_RIGHT,
      base::NumberToString16(new_battery_health->GetBatteryWearPercentage()),
      base::NumberToString16(new_battery_health->GetCycleCount()));

  std::u16string accessibility_extra_details = l10n_util::GetStringFUTF16(
      IDS_ASH_BATTERY_STATUS_IN_LAUNCHER_EXTRA_DETAILS_ACCESSIBILITY_LABEL,
      base::NumberToString16(new_battery_health->GetBatteryWearPercentage()),
      base::NumberToString16(new_battery_health->GetCycleCount()));

  std::u16string accessibility_label_details =
      base::JoinString({new_battery_health->GetAccessibilityLabel(),
                        accessibility_extra_details},
                       u". ");

  AnswerCardInfo answer_card_info(new_battery_health->GetBatteryPercentage());
  // The bar chart will turn red if there is less than 20 of battery
  // charge left.
  answer_card_info.SetLowerLimitForBarChart(20);
  answer_card_info.SetExtraDetails(battery_health_info);
  SearchProvider::Results new_results;
  new_results.emplace_back(std::make_unique<BatteryAnswerResult>(
      profile_, last_query_, /*url_path=*/std::string(), diagnostics_icon_,
      relevance_,
      /*title=*/std::u16string(), new_battery_health->GetPowerTime(),
      accessibility_label_details,
      SystemInfoAnswerResult::SystemInfoCategory::kDiagnostics,
      SystemInfoAnswerResult::SystemInfoCardType::kBattery, answer_card_info));
  SwapResults(&new_results);

  battery_health_ = std::move(new_battery_health);
}

void SystemInfoCardProvider::UpdateChromeOsVersion() {
  std::u16string version =
      base::UTF8ToUTF16(version_info::GetVersionStringWithModifier(""));
  std::u16string is_official = l10n_util::GetStringUTF16(
      version_info::IsOfficialBuild() ? IDS_VERSION_UI_OFFICIAL
                                      : IDS_VERSION_UI_UNOFFICIAL);
  std::u16string processor_variation = l10n_util::GetStringUTF16(
      sizeof(void*) == 8 ? IDS_VERSION_UI_64BIT : IDS_VERSION_UI_32BIT);
  std::u16string channel_name = base::UTF8ToUTF16(
      chrome::GetChannelName(chrome::WithExtendedStable(true)));

  std::u16string version_string = l10n_util::GetStringFUTF16(
      IDS_ASH_VERSION_IN_LAUNCHER_MESSAGE, version, is_official, channel_name,
      processor_variation);
  std::u16string description =
      l10n_util::GetStringUTF16(IDS_ASH_VERSION_IN_LAUNCHER_DESCRIPTION);
  std::u16string accessibility_label_details = l10n_util::GetStringFUTF16(
      IDS_ASH_VERSION_IN_LAUNCHER_ACCESSIBILITY_LABEL, version, is_official,
      channel_name, processor_variation);

  AnswerCardInfo answer_card_info(
      ash::SystemInfoAnswerCardDisplayType::kTextCard);
  SearchProvider::Results new_results;
  new_results.emplace_back(std::make_unique<SystemInfoAnswerResult>(
      profile_, last_query_, kAboutChromeOsSectionPath, os_settings_icon_,
      relevance_, version_string, description, accessibility_label_details,
      SystemInfoAnswerResult::SystemInfoCategory::kSettings,
      SystemInfoAnswerResult::SystemInfoCardType::kVersion, answer_card_info));
  SwapResults(&new_results);
}

void SystemInfoCardProvider::UpdateStorageInfo() {
  total_disk_space_calculator_.StartCalculation();
  free_disk_space_calculator_.StartCalculation();
  my_files_size_calculator_.StartCalculation();
  drive_offline_size_calculator_.StartCalculation();
  browsing_data_size_calculator_.StartCalculation();
  crostini_size_calculator_.StartCalculation();
  other_users_size_calculator_.StartCalculation();
}

void SystemInfoCardProvider::StartObservingCalculators() {
  total_disk_space_calculator_.AddObserver(this);
  free_disk_space_calculator_.AddObserver(this);
  my_files_size_calculator_.AddObserver(this);
  drive_offline_size_calculator_.AddObserver(this);
  browsing_data_size_calculator_.AddObserver(this);
  // TODO(b/324478253): Currently, observing `apps_size_calculator_` at
  // construction causes deterministic failure of ArcIntegrationTest on
  // betty-pi-arc (b/329337572) . As apps size is not currently in use, we
  // remove it from the code. If we are interested in the apps size at some
  // point, consider delaying the observing to the first time launcher search is
  // used.
  calculation_state_.set(
      static_cast<int>(SizeCalculator::CalculationType::kAppsExtensions));
  crostini_size_calculator_.AddObserver(this);
  other_users_size_calculator_.AddObserver(this);
}

void SystemInfoCardProvider::StopObservingCalculators() {
  total_disk_space_calculator_.RemoveObserver(this);
  free_disk_space_calculator_.RemoveObserver(this);
  my_files_size_calculator_.RemoveObserver(this);
  drive_offline_size_calculator_.RemoveObserver(this);
  browsing_data_size_calculator_.RemoveObserver(this);
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

  if (total_bytes <= 0 || available_bytes < 0) {
    // We can't get useful information from the storage page if total_bytes <=
    // 0 or available_bytes is less than 0. This is not expected to happen.
    NOTREACHED_IN_MIGRATION()
        << "Unable to retrieve total or available disk space";
    return;
  }
  CreateStorageAnswerCard();
}

void SystemInfoCardProvider::CreateStorageAnswerCard() {
  const int total_space_index =
      static_cast<int>(SizeCalculator::CalculationType::kTotal);
  const int free_disk_space_index =
      static_cast<int>(SizeCalculator::CalculationType::kAvailable);
  int64_t total_bytes = storage_items_total_bytes_[total_space_index];
  int64_t available_bytes = storage_items_total_bytes_[free_disk_space_index];
  int64_t in_use_bytes = total_bytes - available_bytes;
  std::u16string in_use_size = ui::FormatBytes(in_use_bytes);
  std::u16string total_size = ui::FormatBytes(total_bytes);
  std::u16string description = l10n_util::GetStringFUTF16(
      IDS_ASH_STORAGE_STATUS_IN_LAUNCHER_DESCRIPTION, in_use_size, total_size);

  std::u16string accessibility_label_details = l10n_util::GetStringFUTF16(
      IDS_ASH_STORAGE_STATUS_IN_LAUNCHER_ACCESSIBILITY_LABEL, in_use_size,
      total_size);

  AnswerCardInfo answer_card_info(in_use_bytes * 100 / total_bytes);
  SearchProvider::Results new_results;
  new_results.emplace_back(std::make_unique<SystemInfoAnswerResult>(
      profile_, last_query_, kStorageSubpagePath, os_settings_icon_, relevance_,
      /*title=*/std::u16string(), description, accessibility_label_details,
      SystemInfoAnswerResult::SystemInfoCategory::kSettings,
      SystemInfoAnswerResult::SystemInfoCardType::kStorage, answer_card_info));
  SwapResults(&new_results);
}

void SystemInfoCardProvider::AddCpuDataObserver(CpuDataObserver* observer) {
  cpu_observers_.AddObserver(observer);
}

void SystemInfoCardProvider::RemoveCpuDataObserver(CpuDataObserver* observer) {
  cpu_observers_.RemoveObserver(observer);
}

void SystemInfoCardProvider::SetCpuUsageTimerForTesting(
    std::unique_ptr<base::RepeatingTimer> timer) {
  cpu_usage_timer_ = std::move(timer);
}

void SystemInfoCardProvider::AddMemoryObserver(MemoryObserver* observer) {
  memory_observers_.AddObserver(observer);
}

void SystemInfoCardProvider::RemoveMemoryObserver(MemoryObserver* observer) {
  memory_observers_.RemoveObserver(observer);
}

void SystemInfoCardProvider::SetMemoryTimerForTesting(
    std::unique_ptr<base::RepeatingTimer> timer) {
  memory_timer_ = std::move(timer);
}

}  // namespace app_list
