// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/instance_throttle/arc_app_launch_throttle_observer.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

ArcAppListPrefs::AppInfo CreateAppInfoForPackage(
    const std::string package_name) {
  return ArcAppListPrefs::AppInfo(package_name, package_name, "", "", "",
                                  base::Time(), base::Time(), true, true, true,
                                  true, true, true, true);
}

class ArcAppLaunchThrottleObserverTest : public testing::Test {
 public:
  using testing::Test::Test;

 protected:
  ArcAppLaunchThrottleObserver* observer() { return &app_launch_observer_; }

  content::BrowserTaskEnvironment* environment() { return &task_environment_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ArcAppLaunchThrottleObserver app_launch_observer_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppLaunchThrottleObserverTest);
};

TEST_F(ArcAppLaunchThrottleObserverTest, TestConstructDestruct) {}

TEST_F(ArcAppLaunchThrottleObserverTest, TestOnAppLaunchRequested) {
  const ArcAppListPrefs::AppInfo app1 =
      CreateAppInfoForPackage("com.android.app1");
  const ArcAppListPrefs::AppInfo app2 =
      CreateAppInfoForPackage("com.android.app2");
  const ArcAppListPrefs::AppInfo app3 =
      CreateAppInfoForPackage("com.android.app3");
  EXPECT_FALSE(observer()->active());

  // App1 launch requested, observer is active.
  observer()->OnAppLaunchRequested(app1);
  EXPECT_TRUE(observer()->active());

  // App2 launch requested but finishes before App1, observer is still active.
  observer()->OnAppLaunchRequested(app2);
  observer()->OnTaskCreated(0, app2.package_name, "", "");
  EXPECT_TRUE(observer()->active());

  // App3 finishes launch but observer is not waiting for app3, so it is still
  // active.
  observer()->OnTaskCreated(0, app3.package_name, "", "");

  // App1 finishes launch, observer is inactive.
  observer()->OnTaskCreated(0, app1.package_name, "", "");
}

// Check that a launch request expires.
TEST_F(ArcAppLaunchThrottleObserverTest, TestLaunchRequestExpires) {
  const ArcAppListPrefs::AppInfo app =
      CreateAppInfoForPackage("com.android.app");
  EXPECT_FALSE(observer()->active());

  observer()->OnAppLaunchRequested(app);
  EXPECT_TRUE(observer()->active());

  environment()->FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(observer()->active());
}

}  // namespace
}  // namespace arc
