// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/offline_metrics_collector_impl.h"

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/common/pref_names.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_store_utils.h"

namespace offline_pages {

namespace {

using DailyUsageType = OfflineMetricsCollectorImpl::DailyUsageType;
using PrefetchUsageType = OfflineMetricsCollectorImpl::PrefetchUsageType;

void ReportOfflineUsageForOneDayToUma(DailyUsageType usage_type) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.OfflineUsage", usage_type);
}

void ReportNotResilientOfflineUsageForOneDayToUma(DailyUsageType usage_type) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.OfflineUsage.NotOfflineResilient",
                            usage_type);
}

void ReportPrefetchUsageForOneDayToUma(PrefetchUsageType usage_type) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.PrefetchUsage", usage_type);
}

}  // namespace

// static
void OfflineMetricsCollectorImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kOfflineUsageStartObserved, false);
  registry->RegisterBooleanPref(prefs::kOfflineUsageOnlineObserved, false);
  registry->RegisterBooleanPref(prefs::kOfflineUsageOfflineObserved, false);
  registry->RegisterBooleanPref(prefs::kPrefetchUsageEnabledObserved, false);
  registry->RegisterBooleanPref(prefs::kPrefetchUsageFetchObserved, false);
  registry->RegisterBooleanPref(prefs::kPrefetchUsageOpenObserved, false);
  registry->RegisterInt64Pref(prefs::kOfflineUsageTrackingDay, 0L);
  registry->RegisterIntegerPref(prefs::kOfflineUsageUnusedCount, 0);
  registry->RegisterIntegerPref(prefs::kOfflineUsageStartedCount, 0);
  registry->RegisterIntegerPref(prefs::kOfflineUsageOfflineCount, 0);
  registry->RegisterIntegerPref(prefs::kOfflineUsageOnlineCount, 0);
  registry->RegisterIntegerPref(prefs::kOfflineUsageMixedCount, 0);
  registry->RegisterIntegerPref(prefs::kPrefetchUsageEnabledCount, 0);
  registry->RegisterIntegerPref(prefs::kPrefetchUsageFetchedCount, 0);
  registry->RegisterIntegerPref(prefs::kPrefetchUsageOpenedCount, 0);
  registry->RegisterIntegerPref(prefs::kPrefetchUsageMixedCount, 0);
}

OfflineMetricsCollectorImpl::OfflineMetricsCollectorImpl(PrefService* prefs)
    : prefs_(prefs) {}

OfflineMetricsCollectorImpl::~OfflineMetricsCollectorImpl() {}

void OfflineMetricsCollectorImpl::OnAppStartupOrResume() {
  SetTrackingFlag(&chrome_start_observed_);
}

void OfflineMetricsCollectorImpl::OnSuccessfulNavigationOffline() {
  SetTrackingFlag(&offline_navigation_observed_);
}

void OfflineMetricsCollectorImpl::OnSuccessfulNavigationOnline() {
  SetTrackingFlag(&online_navigation_observed_);
}

void OfflineMetricsCollectorImpl::OnPrefetchEnabled() {
  SetTrackingFlag(&prefetch_is_enabled_observed_);
}

void OfflineMetricsCollectorImpl::OnSuccessfulPagePrefetch() {
  SetTrackingFlag(&prefetch_fetch_observed_);
}

void OfflineMetricsCollectorImpl::OnPrefetchedPageOpened() {
  SetTrackingFlag(&prefetch_open_observed_);
}

void OfflineMetricsCollectorImpl::ReportAccumulatedStats() {
  EnsureLoaded();
  int total_day_count =
      unused_days_count_ + started_days_count_ + offline_days_count_ +
      online_days_count_ + mixed_days_count_ + prefetch_enable_count_ +
      prefetch_fetched_count_ + prefetch_opened_count_ + prefetch_mixed_count_;
  // No accumulated daily usage, nothing to report.
  if (total_day_count == 0)
    return;

  for (int i = 0; i < unused_days_count_; ++i)
    ReportOfflineUsageForOneDayToUma(DailyUsageType::kUnused);
  for (int i = 0; i < started_days_count_; ++i)
    ReportOfflineUsageForOneDayToUma(DailyUsageType::kStarted);
  for (int i = 0; i < offline_days_count_; ++i)
    ReportOfflineUsageForOneDayToUma(DailyUsageType::kOffline);
  for (int i = 0; i < online_days_count_; ++i)
    ReportOfflineUsageForOneDayToUma(DailyUsageType::kOnline);
  for (int i = 0; i < mixed_days_count_; ++i)
    ReportOfflineUsageForOneDayToUma(DailyUsageType::kMixed);

  for (int i = 0; i < prefetch_enable_count_; ++i) {
    UMA_HISTOGRAM_BOOLEAN("OfflinePages.PrefetchEnabled", true);
  }

  for (int i = 0; i < prefetch_fetched_count_; ++i)
    ReportPrefetchUsageForOneDayToUma(PrefetchUsageType::kFetchedNewPages);
  for (int i = 0; i < prefetch_opened_count_; ++i)
    ReportPrefetchUsageForOneDayToUma(PrefetchUsageType::kOpenedPages);
  for (int i = 0; i < prefetch_mixed_count_; ++i)
    ReportPrefetchUsageForOneDayToUma(
        PrefetchUsageType::kFetchedAndOpenedPages);

  unused_days_count_ = 0;
  started_days_count_ = 0;
  offline_days_count_ = 0;
  online_days_count_ = 0;
  mixed_days_count_ = 0;
  prefetch_enable_count_ = 0;
  prefetch_fetched_count_ = 0;
  prefetch_opened_count_ = 0;
  prefetch_mixed_count_ = 0;

  SaveToPrefs();
}

void OfflineMetricsCollectorImpl::EnsureLoaded() {
  if (!tracking_day_midnight_.is_null())
    return;

  DCHECK(prefs_);
  chrome_start_observed_ =
      prefs_->GetBoolean(prefs::kOfflineUsageStartObserved);
  offline_navigation_observed_ =
      prefs_->GetBoolean(prefs::kOfflineUsageOfflineObserved);
  online_navigation_observed_ =
      prefs_->GetBoolean(prefs::kOfflineUsageOnlineObserved);

  prefetch_is_enabled_observed_ =
      prefs_->GetBoolean(prefs::kPrefetchUsageEnabledObserved);
  prefetch_fetch_observed_ =
      prefs_->GetBoolean(prefs::kPrefetchUsageFetchObserved);
  prefetch_open_observed_ =
      prefs_->GetBoolean(prefs::kPrefetchUsageOpenObserved);

  tracking_day_midnight_ = prefs_->GetTime(prefs::kOfflineUsageTrackingDay);
  // For the very first run, initialize to current time.
  if (tracking_day_midnight_.is_null())
    tracking_day_midnight_ = OfflineTimeNow().LocalMidnight();

  unused_days_count_ = prefs_->GetInteger(prefs::kOfflineUsageUnusedCount);
  started_days_count_ = prefs_->GetInteger(prefs::kOfflineUsageStartedCount);
  offline_days_count_ = prefs_->GetInteger(prefs::kOfflineUsageOfflineCount);
  online_days_count_ = prefs_->GetInteger(prefs::kOfflineUsageOnlineCount);
  mixed_days_count_ = prefs_->GetInteger(prefs::kOfflineUsageMixedCount);

  prefetch_enable_count_ =
      prefs_->GetInteger(prefs::kPrefetchUsageEnabledCount);
  prefetch_fetched_count_ =
      prefs_->GetInteger(prefs::kPrefetchUsageFetchedCount);
  prefetch_opened_count_ = prefs_->GetInteger(prefs::kPrefetchUsageOpenedCount);
  prefetch_mixed_count_ = prefs_->GetInteger(prefs::kPrefetchUsageMixedCount);
}

void OfflineMetricsCollectorImpl::SaveToPrefs() {
  prefs_->SetBoolean(prefs::kOfflineUsageStartObserved, chrome_start_observed_);
  prefs_->SetBoolean(prefs::kOfflineUsageOfflineObserved,
                     offline_navigation_observed_);
  prefs_->SetBoolean(prefs::kOfflineUsageOnlineObserved,
                     online_navigation_observed_);
  prefs_->SetBoolean(prefs::kPrefetchUsageEnabledObserved,
                     prefetch_is_enabled_observed_);
  prefs_->SetBoolean(prefs::kPrefetchUsageFetchObserved,
                     prefetch_fetch_observed_);
  prefs_->SetBoolean(prefs::kPrefetchUsageOpenObserved,
                     prefetch_open_observed_);
  prefs_->SetTime(prefs::kOfflineUsageTrackingDay, tracking_day_midnight_);
  prefs_->SetInteger(prefs::kOfflineUsageUnusedCount, unused_days_count_);
  prefs_->SetInteger(prefs::kOfflineUsageStartedCount, started_days_count_);
  prefs_->SetInteger(prefs::kOfflineUsageOfflineCount, offline_days_count_);
  prefs_->SetInteger(prefs::kOfflineUsageOnlineCount, online_days_count_);
  prefs_->SetInteger(prefs::kOfflineUsageMixedCount, mixed_days_count_);

  prefs_->SetInteger(prefs::kPrefetchUsageEnabledCount, prefetch_enable_count_);
  prefs_->SetInteger(prefs::kPrefetchUsageFetchedCount,
                     prefetch_fetched_count_);
  prefs_->SetInteger(prefs::kPrefetchUsageOpenedCount, prefetch_opened_count_);
  prefs_->SetInteger(prefs::kPrefetchUsageMixedCount, prefetch_mixed_count_);

  prefs_->CommitPendingWrite();
}

void OfflineMetricsCollectorImpl::SetTrackingFlag(bool* flag) {
  EnsureLoaded();

  bool changed = UpdatePastDaysIfNeeded() || !(*flag);
  *flag = true;

  if (changed)
    SaveToPrefs();
}

bool OfflineMetricsCollectorImpl::UpdatePastDaysIfNeeded() {
  base::Time current_midnight = OfflineTimeNow().LocalMidnight();
  // 1. If days_since_last_update <= 0, continue to accumulate the current day.
  // 2. If days_since_last_update == 1, stats are recorded and initialized for
  //    the current day.
  // 3. If days_since_last_update > 1, we record days_since_last_update-1
  //    'unused' days, stats are recorded and initialized for the current day.
  const int days_since_last_update =
      (current_midnight - tracking_day_midnight_).InDays();
  if (days_since_last_update <= 0)
    return false;

  // The days between the day when tracking was done and the current one are
  // 'unused'.
  const int unused_days = days_since_last_update - 1;
  DCHECK_GE(unused_days, 0);

  // Increment the counter that corresponds to tracked usage.
  if (online_navigation_observed_ && offline_navigation_observed_) {
    mixed_days_count_++;
    ReportNotResilientOfflineUsageForOneDayToUma(DailyUsageType::kMixed);
  } else if (online_navigation_observed_) {
    online_days_count_++;
    ReportNotResilientOfflineUsageForOneDayToUma(DailyUsageType::kOnline);
  } else if (offline_navigation_observed_) {
    offline_days_count_++;
    ReportNotResilientOfflineUsageForOneDayToUma(DailyUsageType::kOffline);
  } else if (chrome_start_observed_) {
    started_days_count_++;
    ReportNotResilientOfflineUsageForOneDayToUma(DailyUsageType::kStarted);
  } else {
    unused_days_count_++;
    ReportNotResilientOfflineUsageForOneDayToUma(DailyUsageType::kUnused);
  }

  if (prefetch_is_enabled_observed_)
    prefetch_enable_count_++;

  if (prefetch_fetch_observed_ && prefetch_open_observed_)
    prefetch_mixed_count_++;
  else if (prefetch_open_observed_)
    prefetch_opened_count_++;
  else if (prefetch_fetch_observed_)
    prefetch_fetched_count_++;

  unused_days_count_ += unused_days;
  for (int i = 0; i < unused_days; ++i)
    ReportNotResilientOfflineUsageForOneDayToUma(DailyUsageType::kUnused);

  // Reset tracking
  chrome_start_observed_ = false;
  offline_navigation_observed_ = false;
  online_navigation_observed_ = false;
  prefetch_is_enabled_observed_ = false;
  prefetch_fetch_observed_ = false;
  prefetch_open_observed_ = false;

  tracking_day_midnight_ = current_midnight;

  return true;
}

}  // namespace offline_pages
