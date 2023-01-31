// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "chrome/browser/ash/power/ml/user_activity_event.pb.h"
#include "chrome/browser/ash/power/ml/user_activity_manager.h"
#include "chrome/browser/ash/power/ml/user_activity_ukm_logger_helpers.h"
#include "chrome/browser/ash/power/ml/user_activity_ukm_logger_impl.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {
namespace power {
namespace ml {

UserActivityUkmLoggerImpl::UserActivityUkmLoggerImpl()
    : ukm_recorder_(ukm::UkmRecorder::Get()) {}

UserActivityUkmLoggerImpl::~UserActivityUkmLoggerImpl() = default;

void UserActivityUkmLoggerImpl::LogActivity(const UserActivityEvent& event) {
  // Bucketize the features defined in UserActivityUkmLoggerBucketizer if
  // present.
  std::map<std::string, int> buckets =
      UserActivityUkmLoggerBucketizer::BucketizeUserActivityEventData(event);

  DCHECK(ukm_recorder_);
  ukm::SourceId source_id = ukm::NoURLSourceId();
  ukm::builders::UserActivity user_activity(source_id);

  const UserActivityEvent::Features& features = event.features();

  user_activity.SetSequenceId(next_sequence_id_++)
      .SetDeviceMode(features.device_mode())
      .SetDeviceType(features.device_type())
      .SetEventLogDuration(buckets[kEventLogDuration])
      .SetEventReason(event.event().reason())
      .SetEventType(event.event().type())
      .SetLastActivityDay(features.last_activity_day())
      .SetLastActivityTime(buckets[kLastActivityTime])
      .SetRecentTimeActive(features.recent_time_active_sec())
      .SetRecentVideoPlayingTime(buckets[kRecentVideoPlayingTime])
      .SetScreenDimmedInitially(features.screen_dimmed_initially())
      .SetScreenDimOccurred(event.event().screen_dim_occurred())
      .SetScreenLockedInitially(features.screen_locked_initially())
      .SetScreenLockOccurred(event.event().screen_lock_occurred())
      .SetScreenOffInitially(features.screen_off_initially())
      .SetScreenOffOccurred(event.event().screen_off_occurred());

  if (features.has_on_to_dim_sec()) {
    user_activity.SetScreenDimDelay(features.on_to_dim_sec());
  }
  if (features.has_dim_to_screen_off_sec()) {
    user_activity.SetScreenDimToOffDelay(features.dim_to_screen_off_sec());
  }

  if (features.has_last_user_activity_time_sec()) {
    user_activity.SetLastUserActivityTime(buckets[kLastUserActivityTime]);
  }
  if (features.has_time_since_last_key_sec()) {
    user_activity.SetTimeSinceLastKey(features.time_since_last_key_sec());
  }
  if (features.has_time_since_last_mouse_sec()) {
    user_activity.SetTimeSinceLastMouse(features.time_since_last_mouse_sec());
  }
  if (features.has_time_since_last_touch_sec()) {
    user_activity.SetTimeSinceLastTouch(features.time_since_last_touch_sec());
  }

  if (features.has_on_battery()) {
    user_activity.SetOnBattery(features.on_battery());
  }

  if (features.has_battery_percent()) {
    user_activity.SetBatteryPercent(buckets[kBatteryPercent]);
  }

  if (features.has_device_management()) {
    user_activity.SetDeviceManagement(features.device_management());
  }

  if (features.has_time_since_video_ended_sec()) {
    user_activity.SetTimeSinceLastVideoEnded(buckets[kTimeSinceLastVideoEnded]);
  }

  if (features.has_key_events_in_last_hour()) {
    user_activity.SetKeyEventsInLastHour(buckets[kKeyEventsInLastHour]);
  }

  if (features.has_mouse_events_in_last_hour()) {
    user_activity.SetMouseEventsInLastHour(buckets[kMouseEventsInLastHour]);
  }

  if (features.has_touch_events_in_last_hour()) {
    user_activity.SetTouchEventsInLastHour(buckets[kTouchEventsInLastHour]);
  }

  user_activity
      .SetPreviousNegativeActionsCount(
          features.previous_negative_actions_count())
      .SetPreviousPositiveActionsCount(
          features.previous_positive_actions_count());

  if (event.has_model_prediction()) {
    const UserActivityEvent::ModelPrediction& model_prediction =
        event.model_prediction();
    user_activity.SetModelResponse(model_prediction.response())
        .SetModelApplied(model_prediction.model_applied());
    if (model_prediction.response() ==
            UserActivityEvent::ModelPrediction::DIM ||
        model_prediction.response() ==
            UserActivityEvent::ModelPrediction::NO_DIM) {
      user_activity
          .SetModelDecisionThreshold(model_prediction.decision_threshold())
          .SetModelInactivityScore(model_prediction.inactivity_score());
    }
  }

  user_activity.Record(ukm_recorder_);

  if (features.has_source_id()) {
    ukm::builders::UserActivityId user_activity_id(features.source_id());
    user_activity_id.SetActivityId(source_id).SetHasFormEntry(
        features.has_form_entry());

    if (features.has_engagement_score()) {
      user_activity_id.SetSiteEngagementScore(features.engagement_score());
    }
    user_activity_id.Record(ukm_recorder_);
  }
}

}  // namespace ml
}  // namespace power
}  // namespace ash
