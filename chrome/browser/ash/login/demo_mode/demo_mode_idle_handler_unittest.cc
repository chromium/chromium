// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_idle_handler.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_window_closer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ash {
namespace {

const base::TimeDelta kReLuanchDemoAppIdleDuration = base::Seconds(90);

}  // namespace

class DemoModeIdleHandlerTest : public testing::Test {
 protected:
  DemoModeIdleHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    window_closer_ = std::make_unique<DemoModeWindowCloser>(
        base::BindRepeating(&DemoModeIdleHandlerTest::MockLaunchDemoModeApp,
                            base::Unretained(this)));

    // OK to unretained `this` since the life cycle of `demo_mode_idle_handler_`
    // is the same as the tests.
    demo_mode_idle_handler_ =
        std::make_unique<DemoModeIdleHandler>(window_closer_.get());
  }
  ~DemoModeIdleHandlerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Profile 1");
  }

  void TearDown() override {
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
    demo_mode_idle_handler_.reset();
    window_closer_.reset();
  }

  void SimulateUserActivity() {
    ui::UserActivityDetector::Get()->HandleExternalUserActivity();
  }

  void FastForwardBy(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  void MockLaunchDemoModeApp() { launch_demo_app_count_++; }

  int get_launch_demo_app_count() { return launch_demo_app_count_; }
  Profile* profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  int launch_demo_app_count_ = 0;
  TestingProfileManager profile_manager_;
  std::unique_ptr<DemoModeWindowCloser> window_closer_;
  std::unique_ptr<DemoModeIdleHandler> demo_mode_idle_handler_;
  raw_ptr<Profile> profile_ = nullptr;
};

TEST_F(DemoModeIdleHandlerTest, CloseAllBrowsers) {
  // Initialize 2 browsers.
  std::unique_ptr<Browser> browser_1 = CreateBrowserWithTestWindowForParams(
      Browser::CreateParams(profile(), /*user_gesture=*/true));
  std::unique_ptr<Browser> browser_2 = CreateBrowserWithTestWindowForParams(
      Browser::CreateParams(profile(), /*user_gesture=*/true));
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2U);

  // Trigger close all browsers by being idle for
  // `kReLuanchDemoAppIdleDuration`.
  SimulateUserActivity();
  FastForwardBy(kReLuanchDemoAppIdleDuration);
  EXPECT_TRUE(static_cast<TestBrowserWindow*>(browser_1->window())->IsClosed());
  EXPECT_TRUE(static_cast<TestBrowserWindow*>(browser_2->window())->IsClosed());
  // `TestBrowserWindow` does not destroy `Browser` when `Close()` is called,
  // but real browser window does. Reset both browsers here to fake this
  // behavior.
  browser_1.reset();
  browser_2.reset();

  EXPECT_EQ(get_launch_demo_app_count(), 1);
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

TEST_F(DemoModeIdleHandlerTest, ReLaunchDemoApp) {
  // Clear all immediate task on main thread.
  FastForwardBy(base::Seconds(1));

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
