// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/domain_diversity_reporter.h"

#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {
// The interval between two successive domain metrics reports.
constexpr base::TimeDelta kDomainDiversityReportingInterval =
    base::TimeDelta::FromDays(1);

// Pref name for the persistent timestamp of the last report. This pref is
// per local profile but not synced.
constexpr char kDomainDiversityReportingTimestamp[] =
    "domain_diversity.last_reporting_timestamp";
}  // namespace

DomainDiversityReporter::DomainDiversityReporter(
    history::HistoryService* history_service,
    PrefService* prefs,
    base::Clock* clock)
    : history_service_(history_service),
      prefs_(prefs),
      clock_(clock),
      history_service_observer_(this) {
  DCHECK_NE(prefs_, nullptr);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&DomainDiversityReporter::MaybeComputeDomainMetrics,
                         weak_ptr_factory_.GetWeakPtr()));
}

DomainDiversityReporter::~DomainDiversityReporter() = default;

// static
void DomainDiversityReporter::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimePref(kDomainDiversityReportingTimestamp, base::Time());
}

void DomainDiversityReporter::MaybeComputeDomainMetrics() {
  if (history_service_->BackendLoaded()) {
    // HistoryService is ready; proceed to start the domain metrics
    // computation task.
    ComputeDomainMetrics();
  }
  // Observe history service and start reporting as soon as
  // the former is ready.
  DCHECK(!history_service_observer_.IsObserving(history_service_));
  history_service_observer_.Add(history_service_);
}

void DomainDiversityReporter::ComputeDomainMetrics() {
  base::Time time_last_report_triggered =
      prefs_->GetTime(kDomainDiversityReportingTimestamp);
  base::Time time_current_report_triggered = clock_->Now();

  if (time_last_report_triggered < time_current_report_triggered) {
    // The lower boundary of all times is set at Unix epoch, since
    // LocalMidnight() may fail on times represented by a very small value
    // (e.g. Windows epoch).
    if (time_last_report_triggered < base::Time::UnixEpoch())
      time_last_report_triggered = base::Time::UnixEpoch();

    if (time_current_report_triggered < base::Time::UnixEpoch())
      time_current_report_triggered = base::Time::UnixEpoch();

    // Will only report up to 7 days x 3 results.
    int number_of_days_to_report = 7;

    // If the last report time is too far back in the past, simply use the
    // highest possible value for |number_of_days_to_report| and skip its
    // computation. This avoids calling LocalMidnight() on some very old
    // timestamp that may cause unexpected behaviors on certain
    // platforms/timezones (see https://crbug.com/1048145).
    // The beginning and the end of a 7-day period may differ by at most
    // 24 * 8 + 1(DST offset) hours; round up to FromDays(9) here.
    if (time_current_report_triggered - time_last_report_triggered <
        base::TimeDelta::FromDays(number_of_days_to_report + 2)) {
      // Compute the number of days that needs to be reported for based on
      // the last report time and current time.
      base::TimeDelta report_time_range =
          time_current_report_triggered.LocalMidnight() -
          time_last_report_triggered.LocalMidnight();

      // Due to daylight saving time, |report_time_range| may not be a multiple
      // of 24 hours. A small time offset is therefore added to
      // |report_time_range| so that the resulting time range is guaranteed to
      // be at least the correct number of days times 24. The number of days to
      // report is capped at 7 days.
      number_of_days_to_report = std::min(
          (report_time_range + base::TimeDelta::FromHours(4)).InDaysFloored(),
          number_of_days_to_report);
    }

    if (number_of_days_to_report >= 1) {
      history_service_->GetDomainDiversity(
          /*report_time=*/time_current_report_triggered,
          /*number_of_days_to_report=*/number_of_days_to_report,
          /*metric_type_bitmask=*/history::kEnableLast1DayMetric |
              history::kEnableLast7DayMetric | history::kEnableLast28DayMetric,
          base::BindOnce(&DomainDiversityReporter::ReportDomainMetrics,
                         weak_ptr_factory_.GetWeakPtr(),
                         time_current_report_triggered),
          &cancelable_task_tracker_);
    }
  }

  // The next reporting task is scheduled to run 24 hours later.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DomainDiversityReporter::ComputeDomainMetrics,
                     weak_ptr_factory_.GetWeakPtr()),
      kDomainDiversityReportingInterval);
}

void DomainDiversityReporter::ReportDomainMetrics(
    base::Time time_current_report_triggered,
    history::DomainDiversityResults result) {
  // An empty DomainDiversityResults indicates that |db_| is null in
  // HistoryBackend.
  if (result.empty())
    return;

  for (auto& result_one_day : result) {
    UMA_HISTOGRAM_COUNTS_1000("History.DomainCount1Day",
                              result_one_day.one_day_metric.value().count);
    UMA_HISTOGRAM_COUNTS_1000("History.DomainCount7Day",
                              result_one_day.seven_day_metric.value().count);
    UMA_HISTOGRAM_COUNTS_1000(
        "History.DomainCount28Day",
        result_one_day.twenty_eight_day_metric.value().count);
  }

  prefs_->SetTime(kDomainDiversityReportingTimestamp,
                  time_current_report_triggered);
}

void DomainDiversityReporter::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  DCHECK_EQ(history_service, history_service_);
  ComputeDomainMetrics();
}

void DomainDiversityReporter::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observer_.RemoveAll();
  cancelable_task_tracker_.TryCancelAll();
}
