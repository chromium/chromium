// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINDS_CORE_FINDS_SERVICE_H_
#define CHROME_BROWSER_FINDS_CORE_FINDS_SERVICE_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/finds/core/finds_metrics.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/proto/features/finds.pb.h"
#include "components/optimization_guide/proto/finds_metadata.pb.h"

namespace history {
class HistoryService;
class QueryResults;
}  // namespace history

namespace notifications {
class NotificationScheduleService;
struct ClientOverview;
}  // namespace notifications

class OptimizationGuideKeyedService;
class PrefRegistrySimple;
class PrefService;

namespace finds {

// Service to interact with the optimization guide to perform Finds related
// inference.
class FindsService : public KeyedService, public base::SupportsUserData {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnOptInCriteriaFulfilled() = 0;
  };

  // Registers the profile prefs used by FindsService.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit FindsService(
      OptimizationGuideKeyedService* opt_guide_service,
      history::HistoryService* history_service,
      PrefService* pref_service,
      notifications::NotificationScheduleService* notification_service);
  ~FindsService() override;

  FindsService(const FindsService&) = delete;
  FindsService& operator=(const FindsService&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  struct Result {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(Status)
    enum class Status {
      kSuccess = 0,
      kHistoryServiceUnavailable = 1,
      kOptimizationGuideUnavailable = 2,
      kEmptyHistory = 3,
      kModelExecutionFailed = 4,
      kResponseParsingFailed = 5,
      kModelExecutionOnCooldown = 6,
      kCancelled = 7,
      kNoThemesFound = 8,
      kNoNonCooldownThemesFound = 9,
      kNoSuggestionsForTheme = 10,
      kFailedToScheduleNotification = 11,
      kDisabledByEnterprisePolicy = 12,
      kModelExecutionDisabledByParam = 13,

      kMaxValue = kModelExecutionDisabledByParam,
    };
    // LINT.ThenChange(//tools/metrics/histograms/metadata/others/enums.xml:FindsResult)
    Status status;
    std::string message;
  };

  void ExecuteModelAndScheduleNotification(
      base::OnceCallback<void(Result)> callback);
  // Records that the user has visited a URL that can be categorized under an
  // eligible finds theme. This will go towards a theme count for URL visits to
  // determine whether the opt in will be shown.
  void RecordThemeURLVisited(
      optimization_guide::proto::FindsMetadata::ThemeType theme_type);
  // Notifies the service that the user has reached the required number of SRP
  // back navigations to trigger the opt in promo.
  void SRPBackNavigationCountForOptInReached();

  // Potentially requests that pending notifications be rescheduled.
  void MaybeRescheduleNotifications();
  // Schedules a test notification using mocked data, bypassing model execution.
  // This is intended for use by the chrome-finds-internals page only. Do not
  // use in production code.
  bool ScheduleNotificationForInternalsPage();

 private:
  friend class FindsServiceTest;

  void CheckFindsNotificationsEnabledAndMaybeExecute();
  void OnHistoryQueryComplete(base::OnceCallback<void(Result)> callback,
                              history::QueryResults results);
  void OnModelExecutionComplete(
      base::OnceCallback<void(Result)> callback,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);
  void OnGetClientOverview(notifications::ClientOverview overview);
  bool ScheduleNotificationWithModelResult(
      const optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme&
          theme);
  void OnCheckAreFindsNotificationsEnabled(bool enabled);
  void NotifyOptInCriteriaFulfilled(FindsOptInTriggerReason reason);

  raw_ptr<OptimizationGuideKeyedService> opt_guide_service_;
  raw_ptr<history::HistoryService> history_service_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<notifications::NotificationScheduleService>
      notification_schedule_service_;
  base::ObserverList<Observer> observers_;
  base::CancelableTaskTracker history_task_tracker_;
  base::flat_map<optimization_guide::proto::FindsMetadata::ThemeType, int>
      theme_url_visit_count_;
  base::WeakPtrFactory<FindsService> weak_ptr_factory_{this};
};

}  // namespace finds

#endif  // CHROME_BROWSER_FINDS_CORE_FINDS_SERVICE_H_
