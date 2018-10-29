// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/history_statistics_reporter.h"

#include <memory>
#include <utility>
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"

namespace {
// Delay between the scheduling and actual computing/reporting of stats.
const int kComputeStatisticsDelaySeconds = 5;

// Pref name for the persistent timestamp of the last stats reporting.
const char kWeeklyStatsReportingTimestamp[] =
    "explore_sites.weekly_stats_reporting_timestamp";
}  // namespace

namespace explore_sites {

// static
void HistoryStatisticsReporter::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kWeeklyStatsReportingTimestamp, base::Time());
}

// TODO(dimich): remove the need to have CancelableTaskTracker (by removing it
// from HistoryService::CountUniqueHostsVisitedLastMonth)
HistoryStatisticsReporter::HistoryStatisticsReporter(
    history::HistoryService* history_service,
    PrefService* prefs,
    base::Clock* clock)
    : history_service_(history_service),
      prefs_(prefs),
      clock_(clock),
      cancelable_task_tracker_(new base::CancelableTaskTracker()),
      history_service_observer_(this),
      weak_ptr_factory_(this) {}

HistoryStatisticsReporter::~HistoryStatisticsReporter() {}

void HistoryStatisticsReporter::ScheduleReportStatistics() {
  // Only try to report stats once per session.
  if (attempted_to_report_once_ || !history_service_ || !prefs_)
    return;
  attempted_to_report_once_ = true;

  // If we've already reported metrics during last week, bail out.
  base::Time last_report_time = prefs_->GetTime(kWeeklyStatsReportingTimestamp);
  if (last_report_time > clock_->Now() - base::TimeDelta::FromDays(7))
    return;
  prefs_->SetTime(kWeeklyStatsReportingTimestamp, clock_->Now());

  base::TimeDelta computeStatisticsDelay =
      base::TimeDelta::FromSeconds(kComputeStatisticsDelaySeconds);

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HistoryStatisticsReporter::MaybeReportStatistics,
                     weak_ptr_factory_.GetWeakPtr()),
      computeStatisticsDelay);
}

void HistoryStatisticsReporter::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  DCHECK(history_service == history_service_);
  ComputeStatistics();
}

void HistoryStatisticsReporter::MaybeReportStatistics() {
  if (history_service_->BackendLoaded()) {
    // HistoryService is already loaded. Continue with Initialization.
    ComputeStatistics();
  } else {
    // Register for HistoryServiceLoading in case HistoryService is not yet
    // ready.
    DCHECK(!history_service_observer_.IsObserving(history_service_));
    history_service_observer_.Add(history_service_);
  }
}

void HistoryStatisticsReporter::ComputeStatistics() {
  history_service_->CountUniqueHostsVisitedLastMonth(
      base::BindRepeating(&HistoryStatisticsReporter::ReportStatistics,
                          weak_ptr_factory_.GetWeakPtr()),
      cancelable_task_tracker_.get());
}

void HistoryStatisticsReporter::ReportStatistics(
    history::HistoryCountResult result) {
  if (!result.success)
    return;
  UMA_HISTOGRAM_COUNTS_1000("ExploreSites.MonthlyHostCount", result.count);
}
}  // namespace explore_sites
