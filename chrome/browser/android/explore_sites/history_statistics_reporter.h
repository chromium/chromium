// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_HISTORY_STATISTICS_REPORTER_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_HISTORY_STATISTICS_REPORTER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace explore_sites {
class HistoryStatisticsReporter : public history::HistoryServiceObserver {
 public:
  // Delay between the scheduling and actual computing/reporting of stats.
  static constexpr base::TimeDelta kComputeStatisticsDelay = base::Seconds(5);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  HistoryStatisticsReporter(history::HistoryService* history_service,
                            PrefService* prefs);

  HistoryStatisticsReporter(const HistoryStatisticsReporter&) = delete;
  HistoryStatisticsReporter& operator=(const HistoryStatisticsReporter&) =
      delete;

  ~HistoryStatisticsReporter() override;

  // Schedules delayed task to compute/report history statistics.
  void ScheduleReportStatistics();

 private:
  // history::HistoryServiceObserver:
  void OnHistoryServiceLoaded(
      history::HistoryService* history_service) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // If needed, computes/reports of history statistics. Uses Prefs to only
  // run weekly (approximately).
  void MaybeReportStatistics();

  void ComputeStatistics();
  void ReportStatistics(history::HistoryCountResult result);

  const raw_ptr<history::HistoryService> history_service_;
  raw_ptr<PrefService> prefs_;

  base::CancelableTaskTracker cancelable_task_tracker_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};
  bool attempted_to_report_once_ = false;
  base::WeakPtrFactory<HistoryStatisticsReporter> weak_ptr_factory_{this};
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_HISTORY_STATISTICS_REPORTER_H_
