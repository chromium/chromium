// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_IDLE_EVENT_NOTIFIER_H_
#define CHROME_BROWSER_ASH_POWER_ML_IDLE_EVENT_NOTIFIER_H_

#include <memory>
#include <optional>

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/power/ml/boot_clock.h"
#include "chrome/browser/ash/power/ml/user_activity_event.pb.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/compositing/video_detector_observer.mojom.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace ash {
namespace power {
namespace ml {

class RecentEventsCounter;

// This is time since midnight in the local time zone and may move back or
// forward when DST starts or stops.
using TimeOfDay = base::TimeDelta;

// IdleEventNotifier listens to signals and notifies its observers when
// ScreenDimImminent is received from PowerManagerClient. This generates an idle
// event.
class IdleEventNotifier : public chromeos::PowerManagerClient::Observer,
                          public ui::UserActivityObserver,
                          public viz::mojom::VideoDetectorObserver {
 public:
  // If suspend duration is greater than this, we reset timestamps used to calc
  // |ActivityData::recent_time_active|. We also merge video-playing sessions
  // that have a pause shorter than this.
  static constexpr base::TimeDelta kIdleDelay = base::Seconds(30);

  // Count number of key, mouse and touch events in the past hour.
  static constexpr auto kUserInputEventsDuration = base::Hours(1);

  // Granularity of input events is per minute.
  static constexpr int kNumUserInputEventsBuckets =
      kUserInputEventsDuration.InMinutes();

  struct ActivityData {
    ActivityData();

    ActivityData(const ActivityData& input_data);

    UserActivityEvent_Features_DayOfWeek last_activity_day =
        UserActivityEvent_Features_DayOfWeek_SUN;

    // The local time of the last activity before an idle event occurs.
    TimeOfDay last_activity_time_of_day;

    // Last user activity time of the sequence of activities ending in the last
    // activity. It could be different from |last_activity_time_of_day|
    // if the last activity is not a user activity (e.g. video). It is unset if
    // there is no user activity before the idle event is fired.
    std::optional<TimeOfDay> last_user_activity_time_of_day;

    // Duration of activity up to the last activity.
    base::TimeDelta recent_time_active;

    // Duration from the last key/mouse/touch to the time when idle event is
    // generated.  It is unset if there is no key/mouse/touch activity before
    // the idle event.
    std::optional<base::TimeDelta> time_since_last_key;
    std::optional<base::TimeDelta> time_since_last_mouse;
    std::optional<base::TimeDelta> time_since_last_touch;
    // How long recent video has been playing.
    base::TimeDelta video_playing_time;
    // Duration from when video ended. It is unset if video did not play
    // (|video_playing_time| = 0).
    std::optional<base::TimeDelta> time_since_video_ended;

    int key_events_in_last_hour = 0;
    int mouse_events_in_last_hour = 0;
    int touch_events_in_last_hour = 0;
  };

  IdleEventNotifier(
      chromeos::PowerManagerClient* power_client,
      ui::UserActivityDetector* detector,
      mojo::PendingReceiver<viz::mojom::VideoDetectorObserver> receiver);

  IdleEventNotifier(const IdleEventNotifier&) = delete;
  IdleEventNotifier& operator=(const IdleEventNotifier&) = delete;

  ~IdleEventNotifier() override;

  // chromeos::PowerManagerClient::Observer overrides:
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // ui::UserActivityObserver overrides:
  void OnUserActivity(const ui::Event* event) override;

  // viz::mojom::VideoDetectorObserver overrides:
  void OnVideoActivityStarted() override;
  void OnVideoActivityEnded() override;

  // Called in UserActivityController::ShouldDeferScreenDim to prepare activity
  // data for making Smart Dim decision.
  ActivityData GetActivityDataAndReset();

  // Get activity data only, do not mutate the class, may be used by machine
  // learning internal page.
  ActivityData GetActivityData() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(IdleEventNotifierTest, CheckInitialValues);
  friend class IdleEventNotifierTest;

  enum class ActivityType {
    USER_OTHER,  // All other user-related activities.
    KEY,
    MOUSE,
    VIDEO,
    TOUCH
  };

  struct ActivityDataInternal;

  ActivityData ConvertActivityData(
      const ActivityDataInternal& internal_data) const;

  // Updates all activity-related timestamps.
  void UpdateActivityData(ActivityType type);

  // Clears timestamps used to calculate |ActivityData::recent_time_active| so
  // that its duration is recalculated after ScreenDimImminent is received or
  // when suspend duration is longer than kIdleDelay.
  // Also clears timestamps for video playing so that duration of video playing
  // will be recalculated.
  void ResetTimestampsForRecentActivity();

  BootClock boot_clock_;

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};
  base::ScopedObservation<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observation_{this};

  // Last-received external power state. Changes are treated as user activity.
  std::optional<power_manager::PowerSupplyProperties_ExternalPower>
      external_power_;

  base::ObserverList<Observer>::Unchecked observers_;

  // Holds activity timestamps while we monitor for idle events. It will be
  // converted to an ActivityData when an idle event is sent out.
  std::unique_ptr<ActivityDataInternal> internal_data_;

  // Whether video is playing.
  bool video_playing_ = false;
  mojo::Receiver<viz::mojom::VideoDetectorObserver> receiver_;

  std::unique_ptr<RecentEventsCounter> key_counter_;
  std::unique_ptr<RecentEventsCounter> mouse_counter_;
  std::unique_ptr<RecentEventsCounter> touch_counter_;
};

}  // namespace ml
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_ML_IDLE_EVENT_NOTIFIER_H_
