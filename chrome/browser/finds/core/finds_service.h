// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINDS_CORE_FINDS_SERVICE_H_
#define CHROME_BROWSER_FINDS_CORE_FINDS_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/proto/features/finds.pb.h"

namespace history {
class HistoryService;
class QueryResults;
}  // namespace history

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

  explicit FindsService(OptimizationGuideKeyedService* opt_guide_service,
                        history::HistoryService* history_service,
                        PrefService* pref_service);
  ~FindsService() override;

  FindsService(const FindsService&) = delete;
  FindsService& operator=(const FindsService&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  struct Result {
    enum class Status {
      kSuccess,
      kHistoryServiceUnavailable,
      kOptimizationGuideUnavailable,
      kEmptyHistory,
      kModelExecutionFailed,
      kResponseParsingFailed,
    };
    Status status;
    std::string message;
  };

  void MarkNotificationShown(PrefService* pref_service);
  void MarkThemeNotInterested(
      PrefService* pref_service,
      optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme::
          ThemeType theme);
  void ExecuteModelAndScheduleNotification(
      base::OnceCallback<void(Result)> callback);

 private:
  void CheckModelCooldownCriteriaAndMaybeExecute();
  void OnHistoryQueryComplete(base::OnceCallback<void(Result)> callback,
                              history::QueryResults results);
  void OnModelExecutionComplete(
      base::OnceCallback<void(Result)> callback,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  raw_ptr<OptimizationGuideKeyedService> opt_guide_service_;
  raw_ptr<history::HistoryService> history_service_;
  raw_ptr<PrefService> pref_service_;
  base::ObserverList<Observer> observers_;
  base::CancelableTaskTracker history_task_tracker_;
  base::WeakPtrFactory<FindsService> weak_ptr_factory_{this};
};

}  // namespace finds

#endif  // CHROME_BROWSER_FINDS_CORE_FINDS_SERVICE_H_
