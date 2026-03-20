// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_service.h"

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/finds/core/finds_features.h"
#include "chrome/browser/finds/core/finds_pref_names.h"
#include "chrome/browser/finds/core/finds_utils.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/finds.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace finds {

namespace {

// The duration of history to look back when gathering URLs for theme
// suggestions.
// TODO(crbug.com/493283477): Align with notif cooldown.
constexpr base::TimeDelta kHistoryLookbackInterval = base::Days(7);

std::string FindsSuggestionResponseToHumanReadableString(
    const optimization_guide::proto::FindsSuggestionResponse& response) {
  std::vector<std::string> lines;
  for (int i = 0; i < response.suggestions_size(); ++i) {
    const auto& theme = response.suggestions(i);
    lines.push_back(base::StringPrintf("Theme: %s (Type: %d, Score: %lld)",
                                       theme.theme_title().c_str(),
                                       static_cast<int>(theme.theme_type()),
                                       static_cast<int64_t>(theme.score())));
    for (int j = 0; j < theme.suggestions_size(); ++j) {
      const auto& suggestion = theme.suggestions(j);
      lines.push_back(base::StringPrintf("  - %s: %s",
                                         suggestion.title().c_str(),
                                         suggestion.target_url().c_str()));
    }
  }
  return base::JoinString(lines, "\n");
}

bool IsModelExecutionCooldownPassed(const PrefService* pref_service) {
  const int64_t last_timestamp_value =
      pref_service->GetInt64(prefs::kFindsModelExecutionLastTimestamp);
  if (last_timestamp_value == 0) {
    return true;
  }

  const base::Time last_execution_time =
      base::Time::FromMillisecondsSinceUnixEpoch(last_timestamp_value);
  return (base::Time::Now() - last_execution_time) >=
         base::Days(
             finds::features::kModelExecutionCooldownDurationInDays.Get());
}

[[maybe_unused]] bool IsThemeCooldownPassed(
    const PrefService* pref_service,
    optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme::
        ThemeType theme) {
  const std::string theme_pref_string = ThemeTypeEnumToString(theme);
  if (theme_pref_string.empty()) {
    // If the theme type is unknown, we cannot determine the cooldown, so return
    // false to avoid processing the request.
    return false;
  }

  const base::DictValue& not_interested_themes =
      pref_service->GetDict(prefs::kFindsNotInterestedThemesLastTimestamp);

  // Dictionary prefs can only store double values at most.
  std::optional<double> last_timestamp_value =
      not_interested_themes.FindDouble(theme_pref_string);
  if (!last_timestamp_value.has_value()) {
    return true;
  }

  // Requires a cast to int64_t since base::Time's internal value is an int64_t,
  // but the pref value is stored as a double.
  const base::Time last_not_interested_time =
      base::Time::FromSecondsSinceUnixEpoch(last_timestamp_value.value());
  return (base::Time::Now() - last_not_interested_time) >=
         base::Days(finds::features::kThemeCooldownDurationInDays.Get());
}

void SetModelExecutionCooldownTimestamp(PrefService* pref_service) {
  pref_service->SetInt64(prefs::kFindsModelExecutionLastTimestamp,
                         base::Time::Now().InMillisecondsSinceUnixEpoch());
}

void SetThemeCooldownTimestamp(
    PrefService* pref_service,
    optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme::
        ThemeType theme) {
  const std::string theme_pref_string = ThemeTypeEnumToString(theme);
  if (theme_pref_string.empty()) {
    // Do not set a pref if the theme type is unknown.
    return;
  }
  // Store as a double since base::DictValue only supports storing doubles, but
  // the value is essentially an int64_t timestamp.
  ScopedDictPrefUpdate update(pref_service,
                              prefs::kFindsNotInterestedThemesLastTimestamp);
  update->Set(theme_pref_string, base::Time::Now().InSecondsFSinceUnixEpoch());
}

}  // namespace

// static
void FindsService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kFindsModelExecutionLastTimestamp, 0);
  // TODO(crbug.com/494040435): Add logic to clean out deprecated themes.
  registry->RegisterDictionaryPref(
      prefs::kFindsNotInterestedThemesLastTimestamp);
}

FindsService::FindsService(OptimizationGuideKeyedService* opt_guide_service,
                           history::HistoryService* history_service,
                           PrefService* pref_service)
    : opt_guide_service_(opt_guide_service),
      history_service_(history_service),
      pref_service_(pref_service) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FindsService::CheckModelCooldownCriteriaAndMaybeExecute,
                     weak_ptr_factory_.GetWeakPtr()));
}

FindsService::~FindsService() {
  history_task_tracker_.TryCancelAll();
}

void FindsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FindsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FindsService::MarkNotificationShown(PrefService* pref_service) {
  SetModelExecutionCooldownTimestamp(pref_service);
}

void FindsService::MarkThemeNotInterested(
    PrefService* pref_service,
    optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme::
        ThemeType theme) {
  SetThemeCooldownTimestamp(pref_service, theme);
}

void FindsService::ExecuteModelAndScheduleNotification(
    base::OnceCallback<void(Result)> callback) {
  if (!history_service_) {
    std::move(callback).Run({Result::Status::kHistoryServiceUnavailable,
                             "Error: HistoryService not available."});
    return;
  }

  if (!opt_guide_service_) {
    std::move(callback).Run(
        {Result::Status::kOptimizationGuideUnavailable,
         "Error: OptimizationGuideKeyedService not available."});
    return;
  }

  history::QueryOptions options;
  options.begin_time = base::Time::Now() - kHistoryLookbackInterval;

  history_service_->QueryHistory(
      std::u16string(), options,
      base::BindOnce(&FindsService::OnHistoryQueryComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      &history_task_tracker_);
}

void FindsService::CheckModelCooldownCriteriaAndMaybeExecute() {
  if (IsModelExecutionCooldownPassed(pref_service_)) {
    ExecuteModelAndScheduleNotification(base::DoNothing());
  }
}

void FindsService::OnHistoryQueryComplete(
    base::OnceCallback<void(Result)> callback,
    history::QueryResults results) {
  if (!opt_guide_service_) {
    std::move(callback).Run(
        {Result::Status::kOptimizationGuideUnavailable,
         "Error: OptimizationGuideKeyedService not available."});
    return;
  }

  if (results.empty()) {
    std::move(callback).Run({Result::Status::kEmptyHistory,
                             "Error: No history available to suggest themes."});
    return;
  }

  optimization_guide::proto::FindsSuggestionRequest request;
  for (const auto& result : results) {
    auto* entry = request.add_entries();
    entry->set_visit_time_usec(
        (result.visit_time() - base::Time::UnixEpoch()).InMicroseconds());
    entry->set_title(base::UTF16ToUTF8(result.title()));
    entry->set_url(result.url().spec());
  }

  opt_guide_service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kFinds, request, {},
      base::BindOnce(&FindsService::OnModelExecutionComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FindsService::OnModelExecutionComplete(
    base::OnceCallback<void(Result)> callback,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (!result.response.has_value()) {
    std::string error_message =
        base::StringPrintf("Model execution failed. Error code: %d",
                           static_cast<int>(result.response.error().error()));
    std::move(callback).Run(
        {Result::Status::kModelExecutionFailed, error_message});
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::FindsSuggestionResponse>(
      result.response.value());
  if (response) {
    std::move(callback).Run(
        {Result::Status::kSuccess,
         FindsSuggestionResponseToHumanReadableString(*response)});
  } else {
    std::move(callback).Run(
        {Result::Status::kResponseParsingFailed,
         "Model execution successful, but failed to parse response."});
  }
}

}  // namespace finds
