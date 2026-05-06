// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_service.h"

#include <algorithm>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/finds/android/finds_service_android.h"
#endif
#include "chrome/browser/finds/core/finds_features.h"
#include "chrome/browser/finds/core/finds_metrics.h"
#include "chrome/browser/finds/core/finds_pref_names.h"
#include "chrome/browser/finds/core/finds_utils.h"
#include "chrome/browser/notifications/scheduler/public/client_overview.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/notifications/scheduler/public/schedule_params.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/optimization_guide/proto/features/finds.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/l10n/l10n_util.h"

using SuggestionTheme =
    optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme;

namespace finds {

namespace {


// If the finds::features::kNotificationStartTimeMinutes param is set to less
// than this threshold, it is considered to be for testing and will bypass the
// notification scheduling throttling safeguards.
constexpr int kThresholdMinutesForTesting = 5;

std::string FindsSuggestionResponseToHumanReadableString(
    const optimization_guide::proto::FindsSuggestionResponse& response) {
  std::vector<std::string> lines;
  for (int i = 0; i < response.suggested_themes_size(); ++i) {
    const auto& theme = response.suggested_themes(i);
    lines.push_back(base::StringPrintf(
        "Theme: %s (Type: %d, Score: %lld)", theme.theme_title().c_str(),
        static_cast<int>(theme.theme_type()),
        static_cast<int64_t>(theme.theme_score())));
    for (int j = 0; j < theme.theme_suggested_contents_size(); ++j) {
      const auto& suggestion = theme.theme_suggested_contents(j);
      lines.push_back(base::StringPrintf("  - %s: %s",
                                         suggestion.content_title().c_str(),
                                         suggestion.content_url().c_str()));
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
         GetModelExecutionCooldownDurationTimeDelta();
}

bool IsThemeCooldownPassed(const PrefService* pref_service,
                           SuggestionTheme::ThemeType theme) {
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

const SuggestionTheme* GetHighestScoredThemeIfPossible(
    PrefService* pref_service,
    const ::google::protobuf::RepeatedPtrField<SuggestionTheme>&
        suggestion_themes) {
  // Sort the suggestion themes by best score.
  std::vector<const SuggestionTheme*> sorted_themes;
  for (const auto& theme : suggestion_themes) {
    sorted_themes.push_back(&theme);
  }
  std::sort(sorted_themes.begin(), sorted_themes.end(),
            [](const SuggestionTheme* a, const SuggestionTheme* b) {
              return a->theme_score() > b->theme_score();
            });

  for (const auto* theme : sorted_themes) {
    if (theme->theme_suggested_contents().empty()) {
      continue;
    }
    if (IsThemeCooldownPassed(pref_service, theme->theme_type())) {
      return theme;
    }
  }
  return nullptr;
}

notifications::ScheduleParams GetCurrentScheduleParams() {
  // Setup the schedule params to either be for testing or for the standard base
  // use case of 2-4 hours (accounting for a window time of 2 hours).
  notifications::ScheduleParams schedule_params;
  schedule_params.priority =
      (finds::features::kNotificationStartTimeMinutes.Get() <
       kThresholdMinutesForTesting)
          ? notifications::ScheduleParams::Priority::kNoThrottle
          : notifications::ScheduleParams::Priority::kLow;
  schedule_params.deliver_time_start =
      base::Time::Now() +
      base::Minutes(finds::features::kNotificationStartTimeMinutes.Get());
  schedule_params.deliver_time_end =
      base::Time::Now() +
      base::Minutes(finds::features::kNotificationStartTimeMinutes.Get()) +
      base::Minutes(finds::features::kNotificationWindowTimeMinutes.Get());
  return schedule_params;
}

notifications::NotificationData GetNotificationData(
    const SuggestionTheme::SuggestedContent& suggestion,
    SuggestionTheme::ThemeType theme_type) {
  notifications::NotificationData data;
  data.title = base::UTF8ToUTF16(suggestion.content_title());
  data.message = base::UTF8ToUTF16(suggestion.content_description());
  data.custom_data[notifications::kChromeFindsNotificationsUrl] =
      suggestion.content_url();
  data.custom_data[notifications::kChromeFindsNotificationsThemeType] =
      base::NumberToString(static_cast<int>(theme_type));
  data.buttons.clear();

  notifications::NotificationData::Button open_chrome_button;
  open_chrome_button.type = notifications::ActionButtonType::kHelpful;
  open_chrome_button.id = notifications::kDefaultHelpfulButtonId;
  open_chrome_button.text = l10n_util::GetStringUTF16(
      IDS_CHROME_FINDS_NOTIFICATIONS_HELPFUL_BUTTON_TEXT);
  data.buttons.emplace_back(std::move(open_chrome_button));

  notifications::NotificationData::Button not_interested_button;
  not_interested_button.type = notifications::ActionButtonType::kUnhelpful;
  not_interested_button.id = notifications::kDefaultUnhelpfulButtonId;
  not_interested_button.text = l10n_util::GetStringUTF16(
      IDS_CHROME_FINDS_NOTIFICATIONS_UNHELPFUL_BUTTON_TEXT);
  data.buttons.emplace_back(std::move(not_interested_button));

  return data;
}

void RecordFindsResultAndRunCallback(
    base::OnceCallback<void(FindsService::Result)> callback,
    FindsService::Result result) {
  base::UmaHistogramEnumeration("Finds.Result", result.status);
  if (callback) {
    std::move(callback).Run(std::move(result));
  }
}

}  // namespace

// static
void FindsService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kFindsModelExecutionLastTimestamp, 0);
  // TODO(crbug.com/494040435): Add logic to clean out deprecated themes.
  registry->RegisterDictionaryPref(
      prefs::kFindsNotInterestedThemesLastTimestamp);
  registry->RegisterBooleanPref(prefs::kFindsOptInPromoUserInteracted, false);
  registry->RegisterIntegerPref(prefs::kFindsOptInPromoShownCount, 0);
  registry->RegisterInt64Pref(prefs::kFindsOptInPromoLastShownTimestamp, 0);
}

FindsService::FindsService(
    OptimizationGuideKeyedService* opt_guide_service,
    history::HistoryService* history_service,
    PrefService* pref_service,
    notifications::NotificationScheduleService* notification_schedule_service)
    : opt_guide_service_(opt_guide_service),
      history_service_(history_service),
      pref_service_(pref_service),
      notification_schedule_service_(notification_schedule_service) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FindsService::CheckFindsNotificationsEnabledAndMaybeExecute,
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

void FindsService::ExecuteModelAndScheduleNotification(
    base::OnceCallback<void(Result)> callback) {
  if (!IsAllowedByEnterprisePolicy(pref_service_)) {
    RecordFindsResultAndRunCallback(
        std::move(callback), {Result::Status::kDisabledByEnterprisePolicy,
                              "Error: Feature disabled by enterprise policy."});
    return;
  }

  if (finds::features::kBlockModelExecution.Get()) {
    RecordFindsResultAndRunCallback(
        std::move(callback),
        {Result::Status::kModelExecutionDisabledByParam,
         "Error: Model execution disabled by feature parameter."});
    return;
  }

  if (!IsModelExecutionCooldownPassed(pref_service_)) {
    RecordFindsResultAndRunCallback(std::move(callback),
                                    {Result::Status::kModelExecutionOnCooldown,
                                     "Error: Model execution is on cooldown."});
    return;
  }

  if (!history_service_) {
    RecordFindsResultAndRunCallback(std::move(callback),
                                    {Result::Status::kHistoryServiceUnavailable,
                                     "Error: HistoryService not available."});
    return;
  }

  if (!opt_guide_service_) {
    RecordFindsResultAndRunCallback(
        std::move(callback),
        {Result::Status::kOptimizationGuideUnavailable,
         "Error: OptimizationGuideKeyedService not available."});
    return;
  }

  history::QueryOptions options;
  options.begin_time =
      base::Time::Now() - GetModelExecutionCooldownDurationTimeDelta();
  options.max_count = finds::features::kMaxHistoryEntries.Get();

  history_service_->QueryHistory(
      std::u16string(), options,
      base::BindOnce(&FindsService::OnHistoryQueryComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      &history_task_tracker_);
}

void FindsService::RecordThemeURLVisited(
    optimization_guide::proto::FindsMetadata::ThemeType theme_type) {
  if (!IsAllowedByEnterprisePolicy(pref_service_)) {
    return;
  }

  if (theme_type == optimization_guide::proto::FindsMetadata::UNKNOWN) {
    return;
  }

  // Increment the theme url visit count for the given theme type and notify
  // observers if the threshold is met.
  theme_url_visit_count_[theme_type]++;
  if (theme_url_visit_count_[theme_type] >=
      finds::features::kThemeUrlVisitCountForOptIn.Get()) {
    NotifyOptInCriteriaFulfilled(FindsOptInTriggerReason::kThemeUrlVisitCount);

    // Reset the count for the theme type.
    theme_url_visit_count_[theme_type] = 0;
  }
}

void FindsService::SRPBackNavigationCountForOptInReached() {
  if (!IsAllowedByEnterprisePolicy(pref_service_)) {
    return;
  }

  NotifyOptInCriteriaFulfilled(
      FindsOptInTriggerReason::kSrpBackNavigationCount);
}

void FindsService::MaybeRescheduleNotifications() {
  if (!notification_schedule_service_) {
    return;
  }
  notification_schedule_service_->GetClientOverview(
      notifications::SchedulerClientType::kChromeFinds,
      base::BindOnce(&FindsService::OnGetClientOverview,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool FindsService::ScheduleNotificationForInternalsPage() {
  // Mock a suggestion model response and schedule notification.
  optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme theme;
  theme.set_theme_type(optimization_guide::proto::FindsSuggestionResponse::
                           SuggestionTheme::SHOPPING);
  theme.set_theme_title("Internals Test Theme");

  auto* suggestion = theme.add_theme_suggested_contents();
  suggestion->set_content_title("Test Notification");
  suggestion->set_content_description(
      "This is a test notification from the internals page.");
  suggestion->set_content_url("https://www.google.com");

  return ScheduleNotificationWithModelResult(theme);
}

void FindsService::CheckFindsNotificationsEnabledAndMaybeExecute() {
#if BUILDFLAG(IS_ANDROID)
  if (!IsAllowedByEnterprisePolicy(pref_service_)) {
    return;
  }
  FindsServiceAndroid::CheckAreFindsNotificationsEnabledAndroid(
      base::BindOnce(&FindsService::OnCheckAreFindsNotificationsEnabled,
                     weak_ptr_factory_.GetWeakPtr()));
#else
  ExecuteModelAndScheduleNotification(base::DoNothing());
#endif
}

void FindsService::OnHistoryQueryComplete(
    base::OnceCallback<void(Result)> callback,
    history::QueryResults results) {
  if (!opt_guide_service_) {
    RecordFindsResultAndRunCallback(
        std::move(callback),
        {Result::Status::kOptimizationGuideUnavailable,
         "Error: OptimizationGuideKeyedService not available."});
    return;
  }

  if (results.empty()) {
    RecordFindsResultAndRunCallback(
        std::move(callback),
        {Result::Status::kEmptyHistory,
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
    RecordFindsResultAndRunCallback(
        std::move(callback),
        {Result::Status::kModelExecutionFailed, error_message});
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::FindsSuggestionResponse>(
      result.response.value());
  if (!response) {
    RecordFindsResultAndRunCallback(
        std::move(callback),
        {Result::Status::kResponseParsingFailed,
         "Model execution successful, but failed to parse response."});
    return;
  }

  if (response->suggested_themes().empty()) {
    RecordFindsResultAndRunCallback(
        std::move(callback),
        {Result::Status::kNoThemesFound, "No themes found."});
    return;
  }

  const SuggestionTheme* best_theme = GetHighestScoredThemeIfPossible(
      pref_service_, response->suggested_themes());
  if (!best_theme) {
    RecordFindsResultAndRunCallback(
        std::move(callback),
        {Result::Status::kNoNonCooldownThemesFound,
         "No themes found that passed cooldown criteria."});
    return;
  }

  // Shouldn't trigger since empty themes are filtered by
  // GetHighestScoredThemeIfPossible.
  if (best_theme->theme_suggested_contents().empty()) {
    RecordFindsResultAndRunCallback(
        std::move(callback), {Result::Status::kNoSuggestionsForTheme,
                              "No suggestions available for this theme."});
    return;
  }

  bool schedule_success = ScheduleNotificationWithModelResult(*best_theme);
  if (!schedule_success) {
    RecordFindsResultAndRunCallback(
        std::move(callback), {Result::Status::kFailedToScheduleNotification,
                              "Could not schedule notification."});
    return;
  }

  RecordFindsResultAndRunCallback(
      std::move(callback),
      {Result::Status::kSuccess,
       FindsSuggestionResponseToHumanReadableString(*response)});
}

void FindsService::OnGetClientOverview(notifications::ClientOverview overview) {
  if (overview.scheduled_notifications.empty()) {
    return;
  }

  // There should only ever be 1 notification scheduled at a time for finds.
  DCHECK_EQ(overview.scheduled_notifications.size(), 1u);
  const auto* entry = overview.scheduled_notifications[0];
  notifications::NotificationData data = entry->notification_data;
  notifications::ScheduleParams params = GetCurrentScheduleParams();

  notification_schedule_service_->DeleteNotifications(
      notifications::SchedulerClientType::kChromeFinds);
  notification_schedule_service_->Schedule(
      std::make_unique<notifications::NotificationParams>(
          notifications::SchedulerClientType::kChromeFinds, std::move(data),
          std::move(params)));
}

bool FindsService::ScheduleNotificationWithModelResult(
    const SuggestionTheme& theme) {
  if (!notification_schedule_service_) {
    return false;
  }

  // Take the first suggestion in the list per theme.
  const auto& suggestion = theme.theme_suggested_contents(0);
  notifications::ScheduleParams scheduler_params = GetCurrentScheduleParams();
  notifications::NotificationData data =
      GetNotificationData(suggestion, theme.theme_type());
  // Shouldn't happen, but proactively delete stale notifications that have been
  // scheduled but not yet sent out.
  notification_schedule_service_->DeleteNotifications(
      notifications::SchedulerClientType::kChromeFinds);
  notification_schedule_service_->Schedule(
      std::make_unique<notifications::NotificationParams>(
          notifications::SchedulerClientType::kChromeFinds, std::move(data),
          std::move(scheduler_params)));
  // Track model execution timestamp to properly cooldown the model from being
  // rerun during the window between scheduling and notification being shown.
  finds::MarkModelExecutionLastTimestamp(pref_service_);

  return true;
}

void FindsService::OnCheckAreFindsNotificationsEnabled(bool enabled) {
  if (enabled) {
    ExecuteModelAndScheduleNotification(base::DoNothing());
  }
}

void FindsService::NotifyOptInCriteriaFulfilled(
    FindsOptInTriggerReason reason) {
  for (auto& observer : observers_) {
    observer.OnOptInCriteriaFulfilled();
  }
  finds::RecordOptInCriteriaFulfilled(reason);
}

}  // namespace finds
