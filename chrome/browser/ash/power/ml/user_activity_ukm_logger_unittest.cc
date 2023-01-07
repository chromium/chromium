// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/user_activity_ukm_logger_impl.h"

#include <memory>
#include <vector>

#include "chrome/browser/ash/power/ml/user_activity_event.pb.h"
#include "chrome/browser/ash/power/ml/user_activity_manager.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {
namespace ml {

using ukm::builders::UserActivity;
using ukm::builders::UserActivityId;

class UserActivityUkmLoggerTest : public testing::Test {
 public:
  UserActivityUkmLoggerTest() {
    // These values are arbitrary but must correspond with the values
    // in CheckUserActivityValues.
    UserActivityEvent::Event* event = user_activity_event_.mutable_event();
    event->set_log_duration_sec(395);
    event->set_reason(UserActivityEvent::Event::USER_ACTIVITY);
    event->set_type(UserActivityEvent::Event::REACTIVATE);
    event->set_screen_dim_occurred(true);
    event->set_screen_off_occurred(true);
    event->set_screen_lock_occurred(true);

    // In the order of metrics names in ukm.
    UserActivityEvent::Features* features =
        user_activity_event_.mutable_features();
    features->set_battery_percent(96.0);
    features->set_device_management(UserActivityEvent::Features::UNMANAGED);
    features->set_device_mode(UserActivityEvent::Features::CLAMSHELL);
    features->set_device_type(UserActivityEvent::Features::CHROMEBOOK);
    features->set_last_activity_day(UserActivityEvent::Features::MON);
    features->set_last_activity_time_sec(7300);
    features->set_last_user_activity_time_sec(3800);
    features->set_key_events_in_last_hour(20000);
    features->set_recent_time_active_sec(10);
    features->set_previous_negative_actions_count(2);
    features->set_previous_positive_actions_count(1);
    features->set_video_playing_time_sec(800);
    features->set_on_to_dim_sec(100);
    features->set_dim_to_screen_off_sec(200);
    features->set_screen_dimmed_initially(false);
    features->set_screen_locked_initially(false);
    features->set_screen_off_initially(false);
    features->set_time_since_last_mouse_sec(100);
    features->set_time_since_last_touch_sec(311);
    features->set_time_since_video_ended_sec(400);
    features->set_mouse_events_in_last_hour(89);
    features->set_touch_events_in_last_hour(1890);

    UserActivityEvent::ModelPrediction* prediction =
        user_activity_event_.mutable_model_prediction();
    prediction->set_decision_threshold(50);
    prediction->set_inactivity_score(60);
    prediction->set_model_applied(true);
    prediction->set_response(UserActivityEvent::ModelPrediction::NO_DIM);

    user_activity_logger_delegate_ukm_.ukm_recorder_ = &recorder_;
  }

  UserActivityUkmLoggerTest(const UserActivityUkmLoggerTest&) = delete;
  UserActivityUkmLoggerTest& operator=(const UserActivityUkmLoggerTest&) =
      delete;

 protected:
  void LogActivity(const UserActivityEvent& event) {
    user_activity_logger_delegate_ukm_.LogActivity(event);
  }

  void CheckUserActivityValues(const ukm::mojom::UkmEntry* entry) {
    recorder_.ExpectEntryMetric(entry, UserActivity::kEventLogDurationName,
                                395);
    recorder_.ExpectEntryMetric(entry, UserActivity::kEventReasonName,
                                UserActivityEvent::Event::USER_ACTIVITY);
    recorder_.ExpectEntryMetric(entry, UserActivity::kEventTypeName,
                                UserActivityEvent::Event::REACTIVATE);
    recorder_.ExpectEntryMetric(entry, UserActivity::kBatteryPercentName, 95);
    recorder_.ExpectEntryMetric(entry, UserActivity::kDeviceManagementName,
                                UserActivityEvent::Features::UNMANAGED);
    recorder_.ExpectEntryMetric(entry, UserActivity::kDeviceModeName,
                                UserActivityEvent::Features::CLAMSHELL);
    recorder_.ExpectEntryMetric(entry, UserActivity::kDeviceTypeName,
                                UserActivityEvent::Features::CHROMEBOOK);
    recorder_.ExpectEntryMetric(entry, UserActivity::kLastActivityDayName,
                                UserActivityEvent::Features::MON);
    recorder_.ExpectEntryMetric(entry, UserActivity::kKeyEventsInLastHourName,
                                10000);
    recorder_.ExpectEntryMetric(entry, UserActivity::kLastActivityTimeName, 2);
    recorder_.ExpectEntryMetric(entry, UserActivity::kLastUserActivityTimeName,
                                1);
    recorder_.ExpectEntryMetric(entry, UserActivity::kModelAppliedName, 1);
    recorder_.ExpectEntryMetric(entry,
                                UserActivity::kModelDecisionThresholdName, 50);
    recorder_.ExpectEntryMetric(entry, UserActivity::kModelInactivityScoreName,
                                60);
    recorder_.ExpectEntryMetric(entry, UserActivity::kModelResponseName, 1);
    recorder_.ExpectEntryMetric(entry, UserActivity::kMouseEventsInLastHourName,
                                89);
    EXPECT_FALSE(recorder_.EntryHasMetric(entry, UserActivity::kOnBatteryName));
    recorder_.ExpectEntryMetric(
        entry, UserActivity::kPreviousNegativeActionsCountName, 2);
    recorder_.ExpectEntryMetric(
        entry, UserActivity::kPreviousPositiveActionsCountName, 1);

    recorder_.ExpectEntryMetric(entry, UserActivity::kRecentTimeActiveName, 10);
    recorder_.ExpectEntryMetric(entry,
                                UserActivity::kRecentVideoPlayingTimeName, 600);
    recorder_.ExpectEntryMetric(entry, UserActivity::kScreenDimDelayName, 100);
    recorder_.ExpectEntryMetric(entry, UserActivity::kScreenDimmedInitiallyName,
                                false);
    recorder_.ExpectEntryMetric(entry, UserActivity::kScreenDimOccurredName,
                                true);
    recorder_.ExpectEntryMetric(entry, UserActivity::kScreenDimToOffDelayName,
                                200);
    recorder_.ExpectEntryMetric(entry, UserActivity::kScreenLockedInitiallyName,
                                false);
    recorder_.ExpectEntryMetric(entry, UserActivity::kScreenLockOccurredName,
                                true);
    recorder_.ExpectEntryMetric(entry, UserActivity::kScreenOffInitiallyName,
                                false);
    recorder_.ExpectEntryMetric(entry, UserActivity::kScreenOffOccurredName,
                                true);

    recorder_.ExpectEntryMetric(entry, UserActivity::kSequenceIdName, 1);
    EXPECT_FALSE(
        recorder_.EntryHasMetric(entry, UserActivity::kTimeSinceLastKeyName));
    recorder_.ExpectEntryMetric(entry, UserActivity::kTimeSinceLastMouseName,
                                100);
    recorder_.ExpectEntryMetric(entry, UserActivity::kTimeSinceLastTouchName,
                                311);
    recorder_.ExpectEntryMetric(
        entry, UserActivity::kTimeSinceLastVideoEndedName, 360);
    recorder_.ExpectEntryMetric(entry, UserActivity::kTouchEventsInLastHourName,
                                1000);
  }

  UserActivityEvent user_activity_event_;
  ukm::TestUkmRecorder recorder_;

 private:
  UserActivityUkmLoggerImpl user_activity_logger_delegate_ukm_;
};

TEST_F(UserActivityUkmLoggerTest, BasicLogging) {
  auto user_activity_event = user_activity_event_;
  UserActivityEvent::Features* features =
      user_activity_event.mutable_features();
  features->set_source_id(recorder_.GetNewSourceID());
  const GURL kUrl1 = GURL("https://example1.com/");
  features->set_tab_domain(kUrl1.host());
  features->set_engagement_score(90);
  features->set_has_form_entry(false);

  LogActivity(user_activity_event);

  const auto& activity_entries =
      recorder_.GetEntriesByName(UserActivity::kEntryName);
  EXPECT_EQ(1u, activity_entries.size());
  const ukm::mojom::UkmEntry* activity_entry = activity_entries[0];
  CheckUserActivityValues(activity_entry);

  const ukm::SourceId kSourceId = activity_entry->source_id;
  const auto& activity_id_entries =
      recorder_.GetEntriesByName(UserActivityId::kEntryName);
  EXPECT_EQ(1u, activity_id_entries.size());

  const ukm::mojom::UkmEntry* entry = activity_id_entries[0];
  recorder_.ExpectEntryMetric(entry, UserActivityId::kActivityIdName,
                              kSourceId);
  recorder_.ExpectEntryMetric(entry, UserActivityId::kSiteEngagementScoreName,
                              90);
  recorder_.ExpectEntryMetric(entry, UserActivityId::kHasFormEntryName, 0);
}

// Tests what would be logged in Incognito: when source IDs are not provided.
TEST_F(UserActivityUkmLoggerTest, EmptySources) {
  LogActivity(user_activity_event_);

  const auto& activity_entries =
      recorder_.GetEntriesByName(UserActivity::kEntryName);
  EXPECT_EQ(1u, activity_entries.size());
  const ukm::mojom::UkmEntry* activity_entry = activity_entries[0];

  CheckUserActivityValues(activity_entry);

  EXPECT_EQ(0u, recorder_.GetEntriesByName(UserActivityId::kEntryName).size());
}

TEST_F(UserActivityUkmLoggerTest, TwoUserActivityEvents) {
  // A second event will be logged. Values correspond with the checks below.
  UserActivityEvent user_activity_event2;
  UserActivityEvent::Event* event = user_activity_event2.mutable_event();
  event->set_log_duration_sec(35);
  event->set_reason(UserActivityEvent::Event::IDLE_SLEEP);
  event->set_type(UserActivityEvent::Event::TIMEOUT);

  UserActivityEvent::Features* features =
      user_activity_event2.mutable_features();
  features->set_battery_percent(86.0);
  features->set_device_management(UserActivityEvent::Features::MANAGED);
  features->set_device_mode(UserActivityEvent::Features::CLAMSHELL);
  features->set_device_type(UserActivityEvent::Features::CHROMEBOOK);
  features->set_last_activity_day(UserActivityEvent::Features::TUE);
  features->set_last_activity_time_sec(7300);
  features->set_last_user_activity_time_sec(3800);
  features->set_recent_time_active_sec(20);
  features->set_on_to_dim_sec(10);
  features->set_dim_to_screen_off_sec(20);
  features->set_time_since_last_mouse_sec(200);

  UserActivityEvent::ModelPrediction* prediction =
      user_activity_event2.mutable_model_prediction();
  prediction->set_model_applied(false);
  prediction->set_response(UserActivityEvent::ModelPrediction::MODEL_ERROR);

  LogActivity(user_activity_event_);
  LogActivity(user_activity_event2);

  const auto& activity_entries =
      recorder_.GetEntriesByName(UserActivity::kEntryName);
  EXPECT_EQ(2u, activity_entries.size());

  // Check the first user activity values.
  CheckUserActivityValues(activity_entries[0]);

  // Check the second user activity values.
  const ukm::mojom::UkmEntry* entry1 = activity_entries[1];
  recorder_.ExpectEntryMetric(entry1, UserActivity::kEventLogDurationName, 35);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kEventReasonName,
                              UserActivityEvent::Event::IDLE_SLEEP);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kEventTypeName,
                              UserActivityEvent::Event::TIMEOUT);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kBatteryPercentName, 85);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kDeviceManagementName,
                              UserActivityEvent::Features::MANAGED);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kDeviceModeName,
                              UserActivityEvent::Features::CLAMSHELL);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kDeviceTypeName,
                              UserActivityEvent::Features::CHROMEBOOK);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kLastActivityDayName,
                              UserActivityEvent::Features::TUE);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kLastActivityTimeName, 2);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kLastUserActivityTimeName,
                              1);
  EXPECT_FALSE(recorder_.EntryHasMetric(entry1, UserActivity::kOnBatteryName));
  recorder_.ExpectEntryMetric(entry1, UserActivity::kRecentTimeActiveName, 20);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kScreenDimDelayName, 10);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kScreenDimToOffDelayName,
                              20);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kSequenceIdName, 2);
  EXPECT_FALSE(
      recorder_.EntryHasMetric(entry1, UserActivity::kTimeSinceLastKeyName));
  recorder_.ExpectEntryMetric(entry1, UserActivity::kTimeSinceLastMouseName,
                              200);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kModelResponseName, 2);
  recorder_.ExpectEntryMetric(entry1, UserActivity::kModelAppliedName, 0);
  EXPECT_FALSE(recorder_.EntryHasMetric(
      entry1, UserActivity::kModelDecisionThresholdName));
  EXPECT_FALSE(recorder_.EntryHasMetric(
      entry1, UserActivity::kModelInactivityScoreName));

  EXPECT_EQ(0u, recorder_.GetEntriesByName(UserActivityId::kEntryName).size());
}

}  // namespace ml
}  // namespace power
}  // namespace ash
