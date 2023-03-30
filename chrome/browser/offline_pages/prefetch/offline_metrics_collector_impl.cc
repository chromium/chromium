// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/offline_metrics_collector_impl.h"

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/common/pref_names.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_store_utils.h"

namespace offline_pages {

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

}  // namespace offline_pages
