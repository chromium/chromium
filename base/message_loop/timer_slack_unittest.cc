// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/timer_slack.h"

#if defined(OS_MAC)
#include "base/message_loop/message_pump_kqueue.h"
#include "base/message_loop/message_pump_mac.h"
#endif  // defined(OS_MAC)
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

#if defined(OS_MAC)
class TestMessagePumpKqueue : public MessagePumpKqueue {
 public:
  size_t set_wakeup_timer_event_calls() {
    return set_wakeup_timer_event_calls_;
  }
  const kevent64_s& last_timer_event() { return last_timer_event_; }

 protected:
  void SetWakeupTimerEvent(const base::TimeTicks& wakeup_time,
                           bool use_slack,
                           kevent64_s* timer_event) override;

 private:
  size_t set_wakeup_timer_event_calls_{0};
  kevent64_s last_timer_event_;
};

void TestMessagePumpKqueue::SetWakeupTimerEvent(
    const base::TimeTicks& wakeup_time,
    bool use_slack,
    kevent64_s* timer_event) {
  // Call through to the super class, then persist what it set.
  MessagePumpKqueue::SetWakeupTimerEvent(wakeup_time, use_slack, timer_event);

  ++set_wakeup_timer_event_calls_;
  last_timer_event_ = *timer_event;
}
#endif  // defined(OS_MAC)

TEST(TimerSlackTest, LudicrousTimerSlackDefaultsOff) {
  EXPECT_FALSE(IsLudicrousTimerSlackEnabled());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500), GetLudicrousTimerSlack());

#if defined(OS_MAC)
  MessagePumpCFRunLoop message_pump_cf_run_loop;
  EXPECT_EQ(
      MessagePumpCFRunLoop::LudicrousSlackSetting::kLudicrousSlackUninitialized,
      message_pump_cf_run_loop.GetLudicrousSlackStateForTesting());

  // Tickle the delay work path.
  const base::TimeTicks now = TimeTicks::Now();
  message_pump_cf_run_loop.ScheduleDelayedWork(now);
  EXPECT_EQ(MessagePumpCFRunLoop::LudicrousSlackSetting::kLudicrousSlackOff,
            message_pump_cf_run_loop.GetLudicrousSlackStateForTesting());

  TestMessagePumpKqueue message_pump_kqueue;
  // Tickle the delay work path.
  message_pump_kqueue.MaybeUpdateWakeupTimerForTesting(now);
  EXPECT_EQ(1u, message_pump_kqueue.set_wakeup_timer_event_calls());
  EXPECT_FALSE(message_pump_kqueue.last_timer_event().fflags & NOTE_LEEWAY);
  EXPECT_FALSE(message_pump_kqueue
                   .GetIsLudicrousTimerSlackEnabledAndNotSuspendedForTesting());
#endif  // defined(OS_MAC)
}

TEST(TimerSlackTest, LudicrousTimerSlackObservesFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      base::features::kLudicrousTimerSlack);

  EXPECT_TRUE(IsLudicrousTimerSlackEnabled());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500), GetLudicrousTimerSlack());

#if defined(OS_MAC)
  MessagePumpCFRunLoop message_pump_cf_run_loop;
  EXPECT_EQ(
      MessagePumpCFRunLoop::LudicrousSlackSetting::kLudicrousSlackUninitialized,
      message_pump_cf_run_loop.GetLudicrousSlackStateForTesting());

  // Tickle the delay work path.
  const base::TimeTicks now = TimeTicks::Now();
  message_pump_cf_run_loop.ScheduleDelayedWork(now);
  EXPECT_EQ(MessagePumpCFRunLoop::LudicrousSlackSetting::kLudicrousSlackOn,
            message_pump_cf_run_loop.GetLudicrousSlackStateForTesting());

  // Validate that suspend works for the CF message loop.
  SuspendLudicrousTimerSlack();
  EXPECT_EQ(
      MessagePumpCFRunLoop::LudicrousSlackSetting::kLudicrousSlackSuspended,
      message_pump_cf_run_loop.GetLudicrousSlackStateForTesting());
  ResumeLudicrousTimerSlack();

  TestMessagePumpKqueue message_pump_kqueue;
  EXPECT_TRUE(message_pump_kqueue
                  .GetIsLudicrousTimerSlackEnabledAndNotSuspendedForTesting());
#endif  // defined(OS_MAC)
}

#if defined(OS_MAC)
TEST(TimerSlackTest, LudicrousTimerSlackResetsTimerOnSuspendResume) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      base::features::kLudicrousTimerSlack);

  TestMessagePumpKqueue message_pump_kqueue;
  // Tickle the delay work path with the same |now| each time.
  const base::TimeTicks now = TimeTicks::Now();
  message_pump_kqueue.MaybeUpdateWakeupTimerForTesting(now);
  EXPECT_EQ(1u, message_pump_kqueue.set_wakeup_timer_event_calls());
  EXPECT_TRUE(message_pump_kqueue.last_timer_event().fflags & NOTE_LEEWAY);
  EXPECT_TRUE(message_pump_kqueue
                  .GetIsLudicrousTimerSlackEnabledAndNotSuspendedForTesting());
  // A second tickle should not set the timer again.
  message_pump_kqueue.MaybeUpdateWakeupTimerForTesting(now);
  EXPECT_EQ(1u, message_pump_kqueue.set_wakeup_timer_event_calls());

  // Now suspend ludicrous slack and verify that the timer is reset.
  SuspendLudicrousTimerSlack();
  message_pump_kqueue.MaybeUpdateWakeupTimerForTesting(now);
  EXPECT_EQ(2u, message_pump_kqueue.set_wakeup_timer_event_calls());
  EXPECT_FALSE(message_pump_kqueue.last_timer_event().fflags & NOTE_LEEWAY);
  EXPECT_FALSE(message_pump_kqueue
                   .GetIsLudicrousTimerSlackEnabledAndNotSuspendedForTesting());
  // A second tickle should not set the timer again.
  message_pump_kqueue.MaybeUpdateWakeupTimerForTesting(now);
  EXPECT_EQ(2u, message_pump_kqueue.set_wakeup_timer_event_calls());

  // Resume and validate again.
  ResumeLudicrousTimerSlack();
  message_pump_kqueue.MaybeUpdateWakeupTimerForTesting(now);
  EXPECT_EQ(3u, message_pump_kqueue.set_wakeup_timer_event_calls());
  EXPECT_TRUE(message_pump_kqueue.last_timer_event().fflags & NOTE_LEEWAY);
  EXPECT_TRUE(message_pump_kqueue
                  .GetIsLudicrousTimerSlackEnabledAndNotSuspendedForTesting());
}

TEST(TimerSlackTest, LudicrousTimerSlackDoesntDoubleCancelOnSuspendToggle) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      base::features::kLudicrousTimerSlack);

  TestMessagePumpKqueue message_pump_kqueue;
  // Tickle the delay work path with the same |now| each time.
  const base::TimeTicks now = TimeTicks::Now();
  message_pump_kqueue.MaybeUpdateWakeupTimerForTesting(now);
  EXPECT_EQ(1u, message_pump_kqueue.set_wakeup_timer_event_calls());
  EXPECT_TRUE(message_pump_kqueue.last_timer_event().fflags & NOTE_LEEWAY);
  EXPECT_TRUE(message_pump_kqueue
                  .GetIsLudicrousTimerSlackEnabledAndNotSuspendedForTesting());

  // Cancel the timed work.
  message_pump_kqueue.MaybeUpdateWakeupTimerForTesting(base::TimeTicks::Max());
  EXPECT_EQ(2u, message_pump_kqueue.set_wakeup_timer_event_calls());

  // Now suspend ludicrous slack and verify that the timer is not reset again.
  SuspendLudicrousTimerSlack();
  message_pump_kqueue.MaybeUpdateWakeupTimerForTesting(base::TimeTicks::Max());
  EXPECT_EQ(2u, message_pump_kqueue.set_wakeup_timer_event_calls());

  // Resume and validate again.
  ResumeLudicrousTimerSlack();
  message_pump_kqueue.MaybeUpdateWakeupTimerForTesting(base::TimeTicks::Max());
  EXPECT_EQ(2u, message_pump_kqueue.set_wakeup_timer_event_calls());
}
#endif  // defined(OS_MAC)

TEST(TimerSlackTest, LudicrousTimerSlackSlackObservesFeatureParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams parameters;
  parameters["slack_ms"] = "12345ms";
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      base::features::kLudicrousTimerSlack, parameters);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(12345), GetLudicrousTimerSlack());
}

TEST(TimerSlackTest, LudicrousTimerSlackSlackSuspendResume) {
  EXPECT_FALSE(base::IsLudicrousTimerSlackSuspended());

  base::SuspendLudicrousTimerSlack();
  EXPECT_TRUE(base::IsLudicrousTimerSlackSuspended());
  base::SuspendLudicrousTimerSlack();
  EXPECT_TRUE(base::IsLudicrousTimerSlackSuspended());
  base::ResumeLudicrousTimerSlack();
  EXPECT_TRUE(base::IsLudicrousTimerSlackSuspended());
  base::ResumeLudicrousTimerSlack();
  EXPECT_FALSE(base::IsLudicrousTimerSlackSuspended());
}

}  // namespace
}  // namespace base
