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

TEST(TimerSlackTest, LudicrousTimerSlackDefaultsOff) {
  EXPECT_FALSE(IsLudicrousTimerSlackEnabled());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500), GetLudicrousTimerSlack());

#if defined(OS_MAC)
  MessagePumpCFRunLoop message_pump_cf_run_loop;
  EXPECT_EQ(
      MessagePumpCFRunLoop::LudicrousSlackSetting::kLudicrousSlackUninitialized,
      message_pump_cf_run_loop.ludicrous_slack_setting());

  // Tickle the delay work path.
  message_pump_cf_run_loop.ScheduleDelayedWork(TimeTicks::Now());
  EXPECT_EQ(MessagePumpCFRunLoop::LudicrousSlackSetting::kLudicrousSlackOff,
            message_pump_cf_run_loop.ludicrous_slack_setting());

  MessagePumpKqueue message_pump_kqueue;
  EXPECT_FALSE(message_pump_kqueue.is_ludicrous_timer_slack_enabled());
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
      message_pump_cf_run_loop.ludicrous_slack_setting());

  // Tickle the delay work path.
  message_pump_cf_run_loop.ScheduleDelayedWork(TimeTicks::Now());
  EXPECT_EQ(MessagePumpCFRunLoop::LudicrousSlackSetting::kLudicrousSlackOn,
            message_pump_cf_run_loop.ludicrous_slack_setting());

  MessagePumpKqueue message_pump_kqueue;
  EXPECT_TRUE(message_pump_kqueue.is_ludicrous_timer_slack_enabled());
#endif  // defined(OS_MAC)
}

TEST(TimerSlackTest, LudicrousTimerSlackSlackObservesFeatureParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams parameters;
  parameters["slack_ms"] = "12345ms";
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      base::features::kLudicrousTimerSlack, parameters);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(12345), GetLudicrousTimerSlack());
}

}  // namespace
}  // namespace base
