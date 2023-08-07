// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/system_info/system_info_answer_result.h"

#include <string>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/diagnostics_ui/url_constants.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/common/search_result_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace app_list {
namespace {

constexpr char kOsSettingsResultPrefix[] = "os-settings://";
using AnswerCardInfo = ::ash::SystemInfoAnswerCardData;

const std::u16string CardTypeString(
    SystemInfoAnswerResult::SystemInfoCardType system_info_card_type) {
  switch (system_info_card_type) {
    case SystemInfoAnswerResult::SystemInfoCardType::kVersion:
      return l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_OS_VERSION_LABEL_IN_LAUNCHER);
    case SystemInfoAnswerResult::SystemInfoCardType::kMemory:
      return l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_MEMORY_LABEL_IN_LAUNCHER);
    case SystemInfoAnswerResult::SystemInfoCardType::kStorage:
      return l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_STORAGE_LABEL_IN_LAUNCHER);
    case SystemInfoAnswerResult::SystemInfoCardType::kCPU:
      return l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_CPU_LABEL_IN_LAUNCHER);
    case SystemInfoAnswerResult::SystemInfoCardType::kBattery:
      return l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_BATTERY_LABEL_IN_LAUNCHER);
  }
}

const std::u16string CategoryString(
    SystemInfoAnswerResult::SystemInfoCategory system_info_category) {
  switch (system_info_category) {
    case SystemInfoAnswerResult::SystemInfoCategory::kDiagnostics:
      return l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_LABEL_DIAGNOSTICS_APP_IN_LAUNCHER);
    case SystemInfoAnswerResult::SystemInfoCategory::kSettings:
      return l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_LABEL_SETTINGS_PAGE_IN_LAUNCHER);
  }
}

}  // namespace

SystemInfoAnswerResult::SystemInfoAnswerResult(
    Profile* profile,
    const std::u16string& query,
    const std::string& url_path,
    const gfx::ImageSkia& icon,
    double relevance_score,
    const std::u16string& title,
    const std::u16string& description,
    const std::u16string& accessibility_label,
    SystemInfoCategory system_info_category,
    SystemInfoCardType system_info_card_type,
    const ash::SystemInfoAnswerCardData& answer_card_info)
    : system_info_category_(system_info_category),
      system_info_card_type_(system_info_card_type),
      answer_card_info_(answer_card_info),
      profile_(profile),
      query_(query),
      url_path_(url_path) {
  SetDisplayType(DisplayType::kAnswerCard);
  set_relevance(relevance_score);
  // TODO(b/278271038): Consider changing all icons in SystemInfoAnswerResult to
  // use ImageModel instead of ImageSkia.
  SetIcon(IconInfo(ui::ImageModel::FromImageSkia(icon),
                   kSystemAnswerCardIconDimension));
  SetCategory(Category::kSettings);
  SetResultType(ResultType::kSystemInfo);
  UpdateTitleAndDetails(title, description, accessibility_label);
  SetMetricsType(ash::SYSTEM_INFO);
  std::string id =
      system_info_category_ == SystemInfoCategory::kSettings
          ? base::StrCat({kOsSettingsResultPrefix, url_path_})
          : base::StrCat({ash::kChromeUIDiagnosticsAppUrl, url_path_});
  set_id(id);
  SetSystemInfoAnswerCardData(answer_card_info);
}

SystemInfoAnswerResult::~SystemInfoAnswerResult() = default;

void SystemInfoAnswerResult::UpdateTitleAndDetails(
    const std::u16string& title,
    const std::u16string& description,
    const std::u16string& accessibility_label) {
  std::vector<TextItem> title_vector;
  title_vector.push_back(CreateStringTextItem(title));
  SetTitleTextVector(title_vector);

  std::vector<TextItem> details_vector;
  details_vector.push_back(CreateStringTextItem(description));
  SetDetailsTextVector(details_vector);

  std::u16string accessibility_label_answer_type_details =
      l10n_util::GetStringFUTF16(IDS_ASH_ACCESSIBILITY_ANSWER_TYPE_IN_LAUNCHER,
                                 CardTypeString(system_info_card_type_));

  std::u16string accessibility_label_open_page =
      l10n_util::GetStringFUTF16(IDS_ASH_ACCESSIBILITY_OPEN_LABEL_IN_LAUNCHER,
                                 CategoryString(system_info_category_));

  std::vector<std::u16string> accessibility_vector;
  for (const std::u16string& text :
       {accessibility_label_answer_type_details, accessibility_label,
        accessibility_label_open_page}) {
    if (!text.empty()) {
      accessibility_vector.emplace_back(text);
    }
  }

  SetAccessibleName(base::JoinString(accessibility_vector, u". "));
}

void SystemInfoAnswerResult::Open(int event_flags) {
  if (system_info_category_ == SystemInfoCategory::kSettings) {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile_,
                                                                 url_path_);
  } else {
    ::ash::SystemAppLaunchParams launch_params;
    launch_params.url = GURL(id());
    ash::LaunchSystemWebAppAsync(profile_, ash::SystemWebAppType::DIAGNOSTICS,
                                 launch_params);
  }
}

void SystemInfoAnswerResult::UpdateBarChartPercentage(
    const double bar_chart_percentage) {
  answer_card_info_.UpdateBarChartPercentage(bar_chart_percentage);
  SetSystemInfoAnswerCardData(answer_card_info_);
}

}  // namespace app_list
