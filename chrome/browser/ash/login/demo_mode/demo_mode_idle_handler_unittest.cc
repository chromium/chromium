// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_idle_handler.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ash {
namespace {

const base::TimeDelta kReLuanchDemoAppIdleDuration = base::Seconds(90);

}  // namespace

class DemoModeIdleHandlerTest : public testing::Test {
 protected:
  DemoModeIdleHandlerTest() {
    // OK to unretained `this` since the life cycle of `demo_mode_idle_handler_`
    // is the same as the tests.
    demo_mode_idle_handler_ = std::make_unique<DemoModeIdleHandler>(
        base::BindRepeating(&DemoModeIdleHandlerTest::MockLaunchDemoModeApp,
                            base::Unretained(this)));
  }
  ~DemoModeIdleHandlerTest() override = default;

  void SimulateUserActivity() {
    ui::UserActivityDetector::Get()->HandleExternalUserActivity();
  }

  void FastForwardBy(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  void MockLaunchDemoModeApp() { launch_demo_app_count_++; }

  int get_launch_demo_app_count() { return launch_demo_app_count_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  int launch_demo_app_count_ = 0;
  std::unique_ptr<DemoModeIdleHandler> demo_mode_idle_handler_;
};

TEST_F(DemoModeIdleHandlerTest, IdleTimeOut) {
  // Mock first user interact with device and idle for
  // `kReLuanchDemoAppIdleDuration`:
  SimulateUserActivity();
  FastForwardBy(kReLuanchDemoAppIdleDuration);
  EXPECT_EQ(get_launch_demo_app_count(), 1);

  // Mock a second user come after device idle. App will not launch if duration
  // between 2 activities are less than `kReLuanchDemoAppIdleDuration`.
  SimulateUserActivity();
  FastForwardBy(kReLuanchDemoAppIdleDuration / 2);
  EXPECT_EQ(get_launch_demo_app_count(), 1);
  SimulateUserActivity();
  FastForwardBy(kReLuanchDemoAppIdleDuration / 2 + base::Seconds(1));
  EXPECT_EQ(get_launch_demo_app_count(), 1);

  // Mock no user activity in `kReLuanchDemoAppIdleDuration` + 1 second:
  FastForwardBy(kReLuanchDemoAppIdleDuration + base::Seconds(1));
  // Expect app is launched again:
  EXPECT_EQ(get_launch_demo_app_count(), 2);

  // Mock another idle session without any user activity:
  FastForwardBy(kReLuanchDemoAppIdleDuration);
  // Expect app is not launched:
  EXPECT_EQ(get_launch_demo_app_count(), 2);
}

}  // namespace ash
