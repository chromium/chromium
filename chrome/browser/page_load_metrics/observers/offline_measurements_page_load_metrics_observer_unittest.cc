// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/offline_measurements_page_load_metrics_observer.h"

#include "base/android/jni_array.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/offline_pages/android/native_j_unittests_jni_headers/OfflineMeasurementsTestHelper_jni.h"
#include "chrome/browser/offline_pages/measurements/proto/system_state.pb.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

using UkmEntry = ukm::builders::OfflineMeasurements;

class OfflineMeasurementsPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<OfflineMeasurementsPageLoadMetricsObserver>());
  }

  void AddSystemStateListToPrefs(
      offline_measurements_system_state::proto::SystemStateList
          system_state_list) {
    std::string encoded_system_state_list;
    system_state_list.SerializeToString(&encoded_system_state_list);

    JNIEnv* env = base::android::AttachCurrentThread();
    Java_OfflineMeasurementsTestHelper_addSystemStateListToPrefs(
        env, base::android::ToJavaByteArray(env, encoded_system_state_list));
  }
};

TEST_F(OfflineMeasurementsPageLoadMetricsObserverTest, RecordUkmOnCommit) {
  // Set up the test data.
  offline_measurements_system_state::proto::SystemStateList system_state_list;

  // The first time the background task runs, the |time_since_last_check_millis|
  // value will not be set.
  offline_measurements_system_state::proto::SystemState* system_state1 =
      system_state_list.add_system_states();
  system_state1->set_user_state(
      offline_measurements_system_state::proto::SystemState::USING_CHROME);
  system_state1->set_probe_result(
      offline_measurements_system_state::proto::SystemState::VALIDATED);
  system_state1->set_is_roaming(false);
  system_state1->set_is_airplane_mode_enabled(false);
  system_state1->set_local_hour_of_day_start(5);

  offline_measurements_system_state::proto::SystemState* system_state2 =
      system_state_list.add_system_states();
  system_state2->set_user_state(
      offline_measurements_system_state::proto::SystemState::NOT_USING_PHONE);
  system_state2->set_probe_result(
      offline_measurements_system_state::proto::SystemState::NO_INTERNET);
  system_state2->set_is_roaming(false);
  system_state2->set_is_airplane_mode_enabled(true);
  system_state2->set_local_hour_of_day_start(10);
  system_state2->set_time_since_last_check_millis(1000);

  offline_measurements_system_state::proto::SystemState* system_state3 =
      system_state_list.add_system_states();
  system_state3->set_user_state(
      offline_measurements_system_state::proto::SystemState::PHONE_OFF);
  system_state3->set_probe_result(
      offline_measurements_system_state::proto::SystemState::SERVER_ERROR);
  system_state3->set_is_roaming(true);
  system_state3->set_is_airplane_mode_enabled(false);
  system_state3->set_local_hour_of_day_start(15);
  system_state3->set_time_since_last_check_millis(2000);

  // If the background task is cancelled, then only the |probe_result| value
  // will be set.
  offline_measurements_system_state::proto::SystemState* system_state4 =
      system_state_list.add_system_states();
  system_state4->set_probe_result(
      offline_measurements_system_state::proto::SystemState::CANCELLED);

  // In some cases, we can encounter an error while checking whether a network
  // is roaming or not. When this happens, we do not set the IsRoaming field.
  offline_measurements_system_state::proto::SystemState* system_state5 =
      system_state_list.add_system_states();
  system_state5->set_user_state(offline_measurements_system_state::proto::
                                    SystemState::USING_PHONE_NOT_CHROME);
  system_state5->set_probe_result(offline_measurements_system_state::proto::
                                      SystemState::UNEXPECTED_RESPONSE);
  system_state5->set_is_airplane_mode_enabled(false);
  system_state5->set_local_hour_of_day_start(20);
  system_state5->set_time_since_last_check_millis(4000);

  // Add the test data to prefs.
  AddSystemStateListToPrefs(system_state_list);

  // Navigate to some page. This should trigger the logging of all persisted
  // data to UKM.
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  NavigateAndCommit(GURL("https://www.example.com"));

  // Checks that the expected values are recorded to UKM.
  auto ukm_entries = ukm_recorder.GetEntries(
      UkmEntry::kEntryName,
      {UkmEntry::kUserStateName, UkmEntry::kProbeResultName,
       UkmEntry::kIsRoamingName, UkmEntry::kIsAirplaneModeEnabledName,
       UkmEntry::kLocalHourOfDayStartName, UkmEntry::kDurationMillisName});

  // Check that Source ID used is ukm::NoURLSourceId() and not the source ID of
  // the page navigated to.
  std::vector<ukm::TestUkmRecorder::HumanReadableUkmMetrics> ukm_metrics;
  for (const auto& ukm_entry : ukm_entries) {
    EXPECT_EQ(ukm_entry.source_id, ukm::NoURLSourceId());
    ukm_metrics.push_back(ukm_entry.metrics);
  }

  // Establish the expected metrics logged to UKM.
  std::vector<ukm::TestUkmRecorder::HumanReadableUkmMetrics>
      expected_ukm_metrics = {
          {
              {UkmEntry::kUserStateName, offline_measurements_system_state::
                                             proto::SystemState::USING_CHROME},
              {UkmEntry::kProbeResultName, offline_measurements_system_state::
                                               proto::SystemState::VALIDATED},
              {UkmEntry::kIsRoamingName, false},
              {UkmEntry::kIsAirplaneModeEnabledName, false},
              {UkmEntry::kLocalHourOfDayStartName, 5},
              {UkmEntry::kDurationMillisName,
               ukm::GetExponentialBucketMinForUserTiming(0)},
          },
          {
              {UkmEntry::kUserStateName,
               offline_measurements_system_state::proto::SystemState::
                   NOT_USING_PHONE},
              {UkmEntry::kProbeResultName, offline_measurements_system_state::
                                               proto::SystemState::NO_INTERNET},
              {UkmEntry::kIsRoamingName, false},
              {UkmEntry::kIsAirplaneModeEnabledName, true},
              {UkmEntry::kLocalHourOfDayStartName, 10},
              {UkmEntry::kDurationMillisName,
               ukm::GetExponentialBucketMinForUserTiming(1000)},
          },
          {
              {UkmEntry::kUserStateName, offline_measurements_system_state::
                                             proto::SystemState::PHONE_OFF},
              {UkmEntry::kProbeResultName,
               offline_measurements_system_state::proto::SystemState::
                   SERVER_ERROR},
              {UkmEntry::kIsRoamingName, true},
              {UkmEntry::kIsAirplaneModeEnabledName, false},
              {UkmEntry::kLocalHourOfDayStartName, 15},
              {UkmEntry::kDurationMillisName,
               ukm::GetExponentialBucketMinForUserTiming(2000)},
          },
          {
              {UkmEntry::kUserStateName,
               offline_measurements_system_state::proto::SystemState::
                   INVALID_USER_STATE},
              {UkmEntry::kProbeResultName, offline_measurements_system_state::
                                               proto::SystemState::CANCELLED},
              {UkmEntry::kIsAirplaneModeEnabledName, false},
              {UkmEntry::kLocalHourOfDayStartName, 0},
              {UkmEntry::kDurationMillisName,
               ukm::GetExponentialBucketMinForUserTiming(0)},
          },
          {
              {UkmEntry::kUserStateName,
               offline_measurements_system_state::proto::SystemState::
                   USING_PHONE_NOT_CHROME},
              {UkmEntry::kProbeResultName,
               offline_measurements_system_state::proto::SystemState::
                   UNEXPECTED_RESPONSE},
              {UkmEntry::kIsAirplaneModeEnabledName, false},
              {UkmEntry::kLocalHourOfDayStartName, 20},
              {UkmEntry::kDurationMillisName,
               ukm::GetExponentialBucketMinForUserTiming(4000)},
          },
      };

  EXPECT_EQ(ukm_metrics, expected_ukm_metrics);
}
