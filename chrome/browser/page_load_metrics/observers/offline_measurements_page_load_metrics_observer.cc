// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/offline_measurements_page_load_metrics_observer.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/offline_pages/android/offline_page_bridge.h"
#include "chrome/browser/offline_pages/measurements/proto/system_state.pb.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

std::unique_ptr<OfflineMeasurementsPageLoadMetricsObserver>
OfflineMeasurementsPageLoadMetricsObserver::CreateIfNeeded() {
  // If the OfflineMeasurementsBackgroundTask feature is disabled, then don't
  // create a OfflineMeasurementsPageLoadMetricsObserver, because then there are
  // no metrics to record.
  if (!base::FeatureList::IsEnabled(
          chrome::android::kOfflineMeasurementsBackgroundTask)) {
    return nullptr;
  }
  return std::make_unique<OfflineMeasurementsPageLoadMetricsObserver>();
}

OfflineMeasurementsPageLoadMetricsObserver::
    ~OfflineMeasurementsPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
OfflineMeasurementsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  // Get the persisted metrics from the OfflineMeasurementsBackgroundTask, and
  // then log the metrics to UKM.
  offline_measurements_system_state::proto::SystemStateList system_state_list =
      offline_pages::android::OfflinePageBridge::
          GetSystemStateListFromOfflineMeasurementsAsString();
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  for (int i = 0; i < system_state_list.system_states_size(); i++) {
    const auto& system_state = system_state_list.system_states(i);

    ukm::builders::OfflineMeasurements offline_measurements_ukm(
        ukm::NoURLSourceId());

    offline_measurements_ukm.SetUserState(system_state.user_state())
        .SetProbeResult(system_state.probe_result())
        .SetIsAirplaneModeEnabled(system_state.is_airplane_mode_enabled())
        .SetLocalHourOfDayStart(system_state.local_hour_of_day_start())
        .SetDurationMillis(ukm::GetExponentialBucketMinForUserTiming(
            system_state.time_since_last_check_millis()));

    if (system_state.has_is_roaming()) {
      // There are cases where we encounter a SecurityException while trying to
      // check whether the network is marked as roaming or not roaming. When
      // this happens, we do not set the IsRoaming field. See crbug/1246848.
      offline_measurements_ukm.SetIsRoaming(system_state.is_roaming());
    }

    offline_measurements_ukm.Record(ukm_recorder);
  }

  offline_pages::android::OfflinePageBridge::
      ReportOfflineMeasurementMetricsToUma();
  return STOP_OBSERVING;
}
