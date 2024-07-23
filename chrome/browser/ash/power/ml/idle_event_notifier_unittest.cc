// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/idle_event_notifier.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"

namespace ash {
namespace power {
namespace ml {

namespace {

base::TimeDelta GetTimeSinceMidnight(base::Time time) {
  return time - time.LocalMidnight();
}

UserActivityEvent_Features_DayOfWeek GetDayOfWeek(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return static_cast<UserActivityEvent_Features_DayOfWeek>(
      exploded.day_of_week);
}

}  // namespace

bool operator==(const IdleEventNotifier::ActivityData& x,
                const IdleEventNotifier::ActivityData& y) {
  return x.last_activity_day == y.last_activity_day &&
         x.last_activity_time_of_day == y.last_activity_time_of_day &&
         x.last_user_activity_time_of_day == y.last_user_activity_time_of_day &&
         x.recent_time_active == y.recent_time_active &&
         x.time_since_last_key == y.time_since_last_key &&
         x.time_since_last_mouse == y.time_since_last_mouse &&
         x.time_since_last_touch == y.time_since_last_touch &&
         x.video_playing_time == y.video_playing_time &&
         x.time_since_video_ended == y.time_since_video_ended &&
         x.key_events_in_last_hour == y.key_events_in_last_hour &&
         x.mouse_events_in_last_hour == y.mouse_events_in_last_hour &&
         x.touch_events_in_last_hour == y.touch_events_in_last_hour;
}

class IdleEventNotifierTest : public testing::Test {
 public:
  IdleEventNotifierTest()
      : scoped_task_env_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  IdleEventNotifierTest(const IdleEventNotifierTest&) = delete;
  IdleEventNotifierTest& operator=(const IdleEventNotifierTest&) = delete;

  ~IdleEventNotifierTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    mojo::PendingRemote<viz::mojom::VideoDetectorObserver> observer;
    idle_event_notifier_ = std::make_unique<IdleEventNotifier>(
        chromeos::PowerManagerClient::Get(), ui::UserActivityDetector::Get(),
        observer.InitWithNewPipeAndPassReceiver());
    ac_power_.set_external_power(
        power_manager::PowerSupplyProperties_ExternalPower_AC);
    disconnected_power_.set_external_power(
        power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  }

  void TearDown() override {
    idle_event_notifier_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  base::test::TaskEnvironment scoped_task_env_;

  std::unique_ptr<IdleEventNotifier> idle_event_notifier_;
  power_manager::PowerSupplyProperties ac_power_;
  power_manager::PowerSupplyProperties disconnected_power_;
};

// After initialization, |external_power_| is not set up.
TEST_F(IdleEventNotifierTest, CheckInitialValues) {
  EXPECT_FALSE(idle_event_notifier_->external_power_);
}

// Lid is opened, followed by an idle event.
TEST_F(IdleEventNotifierTest, LidOpenEventReceived) {
  const base::Time now = base::Time::Now();
  idle_event_notifier_->LidEventReceived(
      chromeos::PowerManagerClient::LidState::OPEN,
      base::TimeTicks::UnixEpoch());
  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = base::TimeDelta();
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

// PowerChanged signal is received but source isn't changed, so it won't change
// ActivityData that gets reported when an idle event is received.
TEST_F(IdleEventNotifierTest, PowerSourceNotChanged) {
  const base::Time now = base::Time::Now();
  idle_event_notifier_->PowerChanged(ac_power_);
  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = base::TimeDelta();
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());

  scoped_task_env_.FastForwardBy(base::Seconds(10));
  idle_event_notifier_->PowerChanged(ac_power_);
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

// PowerChanged signal is received and source is changed, so a different
// ActivityData gets reported when the 2nd idle event is received.
TEST_F(IdleEventNotifierTest, PowerSourceChanged) {
  const base::Time now_1 = base::Time::Now();
  idle_event_notifier_->PowerChanged(ac_power_);
  IdleEventNotifier::ActivityData data_1;
  data_1.last_activity_day = GetDayOfWeek(now_1);
  const base::TimeDelta time_of_day_1 = GetTimeSinceMidnight(now_1);
  data_1.last_activity_time_of_day = time_of_day_1;
  data_1.last_user_activity_time_of_day = time_of_day_1;
  data_1.recent_time_active = base::TimeDelta();
  EXPECT_EQ(data_1, idle_event_notifier_->GetActivityDataAndReset());

  scoped_task_env_.FastForwardBy(base::Seconds(100));
  const base::Time now_2 = base::Time::Now();
  idle_event_notifier_->PowerChanged(disconnected_power_);
  IdleEventNotifier::ActivityData data_2;
  data_2.last_activity_day = GetDayOfWeek(now_2);
  const base::TimeDelta time_of_day_2 = GetTimeSinceMidnight(now_2);
  data_2.last_activity_time_of_day = time_of_day_2;
  data_2.last_user_activity_time_of_day = time_of_day_2;
  data_2.recent_time_active = base::TimeDelta();
  EXPECT_EQ(data_2, idle_event_notifier_->GetActivityDataAndReset());
}

// Short sleep duration does not break up recent time active.
TEST_F(IdleEventNotifierTest, ShortSuspendDone) {
  const base::Time now_1 = base::Time::Now();
  idle_event_notifier_->PowerChanged(ac_power_);

  scoped_task_env_.FastForwardBy(IdleEventNotifier::kIdleDelay / 2);
  idle_event_notifier_->SuspendDone(IdleEventNotifier::kIdleDelay / 2);

  scoped_task_env_.FastForwardBy(base::Seconds(5));
  idle_event_notifier_->PowerChanged(disconnected_power_);
  const base::Time now_2 = base::Time::Now();

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_2);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now_2);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = now_2 - now_1;
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

// Long sleep duration recalc recent time active.
TEST_F(IdleEventNotifierTest, LongSuspendDone) {
  idle_event_notifier_->PowerChanged(ac_power_);

  scoped_task_env_.FastForwardBy(IdleEventNotifier::kIdleDelay +
                                 base::Seconds(10));
  const base::Time now_1 = base::Time::Now();
  idle_event_notifier_->SuspendDone(IdleEventNotifier::kIdleDelay +
                                    base::Seconds(10));

  scoped_task_env_.FastForwardBy(base::Seconds(10));
  const base::Time now_2 = base::Time::Now();
  idle_event_notifier_->PowerChanged(disconnected_power_);

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_2);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now_2);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = now_2 - now_1;
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

TEST_F(IdleEventNotifierTest, UserActivityKey) {
  const base::Time now = base::Time::Now();
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  idle_event_notifier_->OnUserActivity(&key_event);
  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = base::TimeDelta();
  data.time_since_last_key = base::Seconds(10);
  data.key_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

TEST_F(IdleEventNotifierTest, UserActivityMouse) {
  const base::Time now = base::Time::Now();
  ui::MouseEvent mouse_event(ui::EventType::kMouseExited, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);

  idle_event_notifier_->OnUserActivity(&mouse_event);
  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = base::TimeDelta();
  data.time_since_last_mouse = base::Seconds(10);
  data.mouse_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

TEST_F(IdleEventNotifierTest, UserActivityOther) {
  const base::Time now = base::Time::Now();
  ui::GestureEvent gesture_event(
      0, 0, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureTap));

  idle_event_notifier_->OnUserActivity(&gesture_event);
  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = base::TimeDelta();
  scoped_task_env_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

// Two consecutive activities separated by 2sec only. Only 1 idle event with
// different timestamps for the two activities.
TEST_F(IdleEventNotifierTest, TwoQuickUserActivities) {
  const base::Time now_1 = base::Time::Now();
  ui::MouseEvent mouse_event(ui::EventType::kMouseExited, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  idle_event_notifier_->OnUserActivity(&mouse_event);

  scoped_task_env_.FastForwardBy(base::Seconds(2));
  const base::Time now_2 = base::Time::Now();
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  idle_event_notifier_->OnUserActivity(&key_event);

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_2);
  const base::TimeDelta time_of_day = GetTimeSinceMidnight(now_2);
  data.last_activity_time_of_day = time_of_day;
  data.last_user_activity_time_of_day = time_of_day;
  data.recent_time_active = now_2 - now_1;
  data.time_since_last_key = base::Seconds(10);
  data.time_since_last_mouse = base::Seconds(10) + (now_2 - now_1);
  data.key_events_in_last_hour = 1;
  data.mouse_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

TEST_F(IdleEventNotifierTest, ActivityAfterVideoStarts) {
  ui::MouseEvent mouse_event(ui::EventType::kMouseExited, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  const base::Time now_1 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::Seconds(1));
  const base::Time now_2 = base::Time::Now();
  idle_event_notifier_->OnUserActivity(&mouse_event);

  scoped_task_env_.FastForwardBy(base::Seconds(2));
  const base::Time now_3 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_3);
  data.last_activity_time_of_day = GetTimeSinceMidnight(now_3);
  data.last_user_activity_time_of_day = GetTimeSinceMidnight(now_2);
  data.recent_time_active = now_3 - now_1;
  data.time_since_last_mouse = base::Seconds(10) + now_3 - now_2;
  data.video_playing_time = now_3 - now_1;
  data.time_since_video_ended = base::Seconds(10);
  data.mouse_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

TEST_F(IdleEventNotifierTest, IdleEventFieldReset) {
  const base::Time now_1 = base::Time::Now();
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  idle_event_notifier_->OnUserActivity(&key_event);

  scoped_task_env_.FastForwardBy(base::Seconds(10));
  const base::Time now_2 = base::Time::Now();
  ui::MouseEvent mouse_event(ui::EventType::kMouseExited, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  idle_event_notifier_->OnUserActivity(&mouse_event);

  IdleEventNotifier::ActivityData data_1;
  data_1.last_activity_day = GetDayOfWeek(now_2);
  const base::TimeDelta time_of_day_2 = GetTimeSinceMidnight(now_2);
  data_1.last_activity_time_of_day = time_of_day_2;
  data_1.last_user_activity_time_of_day = time_of_day_2;
  data_1.recent_time_active = now_2 - now_1;
  data_1.time_since_last_key = base::Seconds(10) + now_2 - now_1;
  data_1.time_since_last_mouse = base::Seconds(10);
  data_1.key_events_in_last_hour = 1;
  data_1.mouse_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(data_1, idle_event_notifier_->GetActivityDataAndReset());

  idle_event_notifier_->PowerChanged(ac_power_);
  const base::Time now_3 = base::Time::Now();

  IdleEventNotifier::ActivityData data_2;
  data_2.last_activity_day = GetDayOfWeek(now_3);
  const base::TimeDelta time_of_day_3 = GetTimeSinceMidnight(now_3);
  data_2.last_activity_time_of_day = time_of_day_3;
  data_2.last_user_activity_time_of_day = time_of_day_3;
  data_2.recent_time_active = base::TimeDelta();
  data_2.time_since_last_key = base::Seconds(20) + now_3 - now_1;
  data_2.time_since_last_mouse = base::Seconds(20) + now_3 - now_2;
  data_2.key_events_in_last_hour = 1;
  data_2.mouse_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::Seconds(20));
  EXPECT_EQ(data_2, idle_event_notifier_->GetActivityDataAndReset());
}

TEST_F(IdleEventNotifierTest, TwoConsecutiveVideoPlaying) {
  // Two video playing sessions with a gap shorter than kIdleDelay. They are
  // merged into one playing session.
  const base::Time now_1 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::Seconds(2));
  idle_event_notifier_->OnVideoActivityEnded();

  scoped_task_env_.FastForwardBy(IdleEventNotifier::kIdleDelay / 2);
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::Seconds(20));
  const base::Time now_2 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityEnded();

  scoped_task_env_.FastForwardBy(base::Seconds(10));
  const base::Time now_3 = base::Time::Now();
  ui::MouseEvent mouse_event(ui::EventType::kMouseExited, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  idle_event_notifier_->OnUserActivity(&mouse_event);

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_3);
  data.last_activity_time_of_day = GetTimeSinceMidnight(now_3);
  data.last_user_activity_time_of_day = GetTimeSinceMidnight(now_3);
  data.recent_time_active = now_3 - now_1;
  data.time_since_last_mouse = base::Seconds(25);
  data.mouse_events_in_last_hour = 1;
  data.video_playing_time = now_2 - now_1;
  data.time_since_video_ended = base::Seconds(25) + now_3 - now_2;
  scoped_task_env_.FastForwardBy(base::Seconds(25));
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

TEST_F(IdleEventNotifierTest, TwoVideoPlayingFarApartOneIdleEvent) {
  // Two video playing sessions with a gap larger than kIdleDelay.
  const base::Time now_1 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::Seconds(2));
  idle_event_notifier_->OnVideoActivityEnded();

  ui::MouseEvent mouse_event(ui::EventType::kMouseExited, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);

  scoped_task_env_.FastForwardBy(base::Seconds(20));
  idle_event_notifier_->OnUserActivity(&mouse_event);
  scoped_task_env_.FastForwardBy(base::Seconds(20));
  idle_event_notifier_->OnUserActivity(&mouse_event);

  const base::Time now_2 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityStarted();
  scoped_task_env_.FastForwardBy(base::Seconds(20));
  const base::Time now_3 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_3);
  data.last_activity_time_of_day = GetTimeSinceMidnight(now_3);
  data.last_user_activity_time_of_day = GetTimeSinceMidnight(now_2);
  data.recent_time_active = now_3 - now_1;
  data.time_since_last_mouse = base::Seconds(25) + now_3 - now_2;
  data.mouse_events_in_last_hour = 2;
  data.video_playing_time = now_3 - now_2;
  data.time_since_video_ended = base::Seconds(25);
  scoped_task_env_.FastForwardBy(base::Seconds(25));
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

TEST_F(IdleEventNotifierTest, TwoVideoPlayingFarApartTwoIdleEvents) {
  // Two video playing sessions with a gap equal to kIdleDelay. An idle event
  // is generated in between, both video sessions are reported.
  const base::Time now_1 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::Seconds(2));
  const base::Time now_2 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data_1;
  data_1.last_activity_day = GetDayOfWeek(now_2);
  data_1.last_activity_time_of_day = GetTimeSinceMidnight(now_2);
  data_1.recent_time_active = now_2 - now_1;
  data_1.video_playing_time = now_2 - now_1;
  data_1.time_since_video_ended =
      IdleEventNotifier::kIdleDelay + base::Seconds(10);
  scoped_task_env_.FastForwardBy(IdleEventNotifier::kIdleDelay +
                                 base::Seconds(10));
  EXPECT_EQ(data_1, idle_event_notifier_->GetActivityDataAndReset());

  const base::Time now_3 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityStarted();
  scoped_task_env_.FastForwardBy(base::Seconds(20));
  const base::Time now_4 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data_2;
  data_2.last_activity_day = GetDayOfWeek(now_4);
  data_2.last_activity_time_of_day = GetTimeSinceMidnight(now_4);
  data_2.recent_time_active = now_4 - now_3;
  data_2.video_playing_time = now_4 - now_3;
  data_2.time_since_video_ended = base::TimeDelta();
  EXPECT_EQ(data_2, idle_event_notifier_->GetActivityDataAndReset());
}

TEST_F(IdleEventNotifierTest, TwoVideoPlayingSeparatedByAnIdleEvent) {
  // Two video playing sessions with gap shorter than kIdleDelay but separated
  // by an idle event. They are considered as two video sessions.
  const base::Time kNow1 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::Seconds(2));
  const base::Time kNow2 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data_1;
  data_1.last_activity_day = GetDayOfWeek(kNow2);
  data_1.last_activity_time_of_day = GetTimeSinceMidnight(kNow2);
  data_1.recent_time_active = kNow2 - kNow1;
  data_1.video_playing_time = kNow2 - kNow1;
  data_1.time_since_video_ended = base::Seconds(1);
  scoped_task_env_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(data_1, idle_event_notifier_->GetActivityDataAndReset());

  const base::Time kNow3 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityStarted();
  scoped_task_env_.FastForwardBy(base::Seconds(20));
  const base::Time kNow4 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data_2;
  data_2.last_activity_day = GetDayOfWeek(kNow4);
  data_2.last_activity_time_of_day = GetTimeSinceMidnight(kNow4);
  data_2.recent_time_active = kNow4 - kNow3;
  data_2.video_playing_time = kNow4 - kNow3;
  data_2.time_since_video_ended = base::TimeDelta();
  EXPECT_EQ(data_2, idle_event_notifier_->GetActivityDataAndReset());
}

TEST_F(IdleEventNotifierTest, VideoPlayingPausedByShortSuspend) {
  const base::Time now_1 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::Seconds(100));
  const base::Time now_2 = base::Time::Now();
  idle_event_notifier_->SuspendDone(IdleEventNotifier::kIdleDelay / 2);

  scoped_task_env_.FastForwardBy(base::Seconds(20));
  const base::Time now_3 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_3);
  data.last_activity_time_of_day = GetTimeSinceMidnight(now_3);
  data.last_user_activity_time_of_day = GetTimeSinceMidnight(now_2);
  data.recent_time_active = now_3 - now_1;
  data.video_playing_time = now_3 - now_1;
  data.time_since_video_ended = base::TimeDelta();
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

TEST_F(IdleEventNotifierTest, VideoPlayingPausedByLongSuspend) {
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(2 * IdleEventNotifier::kIdleDelay);
  const base::Time now_1 = base::Time::Now();
  idle_event_notifier_->SuspendDone(2 * IdleEventNotifier::kIdleDelay);

  scoped_task_env_.FastForwardBy(base::Seconds(20));
  const base::Time now_2 = base::Time::Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(now_2);
  data.last_activity_time_of_day = GetTimeSinceMidnight(now_2);
  data.last_user_activity_time_of_day = GetTimeSinceMidnight(now_1);
  data.recent_time_active = now_2 - now_1;
  data.video_playing_time = now_2 - now_1;
  data.time_since_video_ended = base::TimeDelta();
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

TEST_F(IdleEventNotifierTest, UserInputEventsOneIdleEvent) {
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  ui::MouseEvent mouse_event(ui::EventType::kMouseExited, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  ui::TouchEvent touch_event(
      ui::EventType::kTouchPressed, gfx::Point(0, 0),
      base::TimeTicks::UnixEpoch(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));

  const base::Time first_activity_time = base::Time::Now();
  // This key event will be too old to be counted.
  idle_event_notifier_->OnUserActivity(&key_event);

  scoped_task_env_.FastForwardBy(base::Seconds(10));
  const base::Time video_start_time = base::Time::Now();
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::Minutes(11));
  const base::Time last_key_time = base::Time::Now();
  idle_event_notifier_->OnUserActivity(&key_event);

  scoped_task_env_.FastForwardBy(base::Minutes(11));
  idle_event_notifier_->OnUserActivity(&mouse_event);

  scoped_task_env_.FastForwardBy(base::Minutes(11));
  idle_event_notifier_->OnUserActivity(&touch_event);

  scoped_task_env_.FastForwardBy(base::Minutes(11));
  idle_event_notifier_->OnUserActivity(&touch_event);

  scoped_task_env_.FastForwardBy(base::Minutes(11));
  const base::Time last_touch_time = base::Time::Now();
  idle_event_notifier_->OnUserActivity(&touch_event);

  scoped_task_env_.FastForwardBy(base::Minutes(11));
  const base::Time last_mouse_time = base::Time::Now();
  idle_event_notifier_->OnUserActivity(&mouse_event);

  scoped_task_env_.FastForwardBy(base::Seconds(10));
  const base::Time video_end_time = base::Time::Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data;
  data.last_activity_day = GetDayOfWeek(video_end_time);
  data.last_activity_time_of_day = GetTimeSinceMidnight(video_end_time);
  data.last_user_activity_time_of_day = GetTimeSinceMidnight(last_mouse_time);
  data.recent_time_active = video_end_time - first_activity_time;
  data.video_playing_time = video_end_time - video_start_time;
  data.time_since_video_ended = base::Seconds(30);
  data.time_since_last_key = base::Seconds(30) + video_end_time - last_key_time;
  data.time_since_last_mouse =
      base::Seconds(30) + video_end_time - last_mouse_time;
  data.time_since_last_touch =
      base::Seconds(30) + video_end_time - last_touch_time;

  data.key_events_in_last_hour = 1;
  data.mouse_events_in_last_hour = 2;
  data.touch_events_in_last_hour = 3;

  scoped_task_env_.FastForwardBy(base::Seconds(30));
  EXPECT_EQ(data, idle_event_notifier_->GetActivityDataAndReset());
}

TEST_F(IdleEventNotifierTest, UserInputEventsTwoIdleEvents) {
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  ui::MouseEvent mouse_event(ui::EventType::kMouseExited, gfx::Point(0, 0),
                             gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  ui::TouchEvent touch_event(
      ui::EventType::kTouchPressed, gfx::Point(0, 0),
      base::TimeTicks::UnixEpoch(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));

  const base::Time now_1 = base::Time::Now();
  idle_event_notifier_->OnUserActivity(&key_event);

  IdleEventNotifier::ActivityData data_1;
  data_1.last_activity_day = GetDayOfWeek(now_1);
  data_1.last_activity_time_of_day = GetTimeSinceMidnight(now_1);
  data_1.last_user_activity_time_of_day = GetTimeSinceMidnight(now_1);
  data_1.recent_time_active = base::TimeDelta();
  data_1.time_since_last_key = base::Seconds(30);
  data_1.key_events_in_last_hour = 1;
  scoped_task_env_.FastForwardBy(base::Seconds(30));
  EXPECT_EQ(data_1, idle_event_notifier_->GetActivityDataAndReset());

  scoped_task_env_.FastForwardBy(base::Minutes(11));
  const base::Time last_key_time = base::Time::Now();
  idle_event_notifier_->OnUserActivity(&key_event);

  scoped_task_env_.FastForwardBy(base::Seconds(10));
  const base::Time video_start_time = base::Time::Now();
  // Keep playing video so we won't run into an idle event.
  idle_event_notifier_->OnVideoActivityStarted();

  scoped_task_env_.FastForwardBy(base::Minutes(11));
  idle_event_notifier_->OnUserActivity(&mouse_event);

  scoped_task_env_.FastForwardBy(base::Minutes(11));
  idle_event_notifier_->OnUserActivity(&touch_event);

  scoped_task_env_.FastForwardBy(base::Minutes(11));
  idle_event_notifier_->OnUserActivity(&touch_event);

  scoped_task_env_.FastForwardBy(base::Minutes(11));
  const base::Time last_touch_time = base::Time::Now();
  idle_event_notifier_->OnUserActivity(&touch_event);

  scoped_task_env_.FastForwardBy(base::Minutes(11));
  const base::Time last_mouse_time = base::Time::Now();
  idle_event_notifier_->OnUserActivity(&mouse_event);

  scoped_task_env_.FastForwardBy(base::Seconds(10));
  const base::Time video_end_time = base::Time::Now();
  idle_event_notifier_->OnVideoActivityEnded();

  IdleEventNotifier::ActivityData data_2;
  data_2.last_activity_day = GetDayOfWeek(video_end_time);
  data_2.last_activity_time_of_day = GetTimeSinceMidnight(video_end_time);
  data_2.last_user_activity_time_of_day = GetTimeSinceMidnight(last_mouse_time);
  data_2.recent_time_active = video_end_time - last_key_time;
  data_2.video_playing_time = video_end_time - video_start_time;
  data_2.time_since_video_ended = base::TimeDelta();
  data_2.time_since_last_key = video_end_time - last_key_time;
  data_2.time_since_last_mouse = video_end_time - last_mouse_time;
  data_2.time_since_last_touch = video_end_time - last_touch_time;

  data_2.key_events_in_last_hour = 1;
  data_2.mouse_events_in_last_hour = 2;
  data_2.touch_events_in_last_hour = 3;

  EXPECT_EQ(data_2, idle_event_notifier_->GetActivityDataAndReset());
}

}  // namespace ml
}  // namespace power
}  // namespace ash
