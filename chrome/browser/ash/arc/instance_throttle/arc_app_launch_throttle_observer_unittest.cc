// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_app_launch_throttle_observer.h"

#include <memory>
#include <string>

#include "ash/components/arc/mojom/compatibility_mode.mojom.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {
namespace {

ArcAppListPrefs::AppInfo CreateAppInfoForPackage(
    const std::string package_name) {
  return ArcAppListPrefs::AppInfo(
      package_name, package_name, "" /* activity */, "" /* intent_uri */,
      "" /* icon_resource_id */, absl::nullopt /* version_name */,
      base::Time() /* last_launch_time */, base::Time() /* install_time */,
      true /* sticky */, true /* notifications_enabled */,
      arc::mojom::ArcResizeLockState::UNDEFINED,
      true /* resize_lock_needs_confirmation */,
      ArcAppListPrefs::WindowLayout(), true /* ready */, true /* suspended */,
      true /* show_in_launcher */, true /* shortcut */, true /* launchable */,
      false /* need_fixup */, absl::nullopt /* app_size_in_bytes */,
      absl::nullopt /* data_size_in_bytes */);
}

class ArcAppLaunchThrottleObserverTest : public testing::Test {
 public:
  using testing::Test::Test;

  ArcAppLaunchThrottleObserverTest(const ArcAppLaunchThrottleObserverTest&) =
      delete;
  ArcAppLaunchThrottleObserverTest& operator=(
      const ArcAppLaunchThrottleObserverTest&) = delete;

 protected:
  ArcAppLaunchThrottleObserver* observer() { return &app_launch_observer_; }

  content::BrowserTaskEnvironment* environment() { return &task_environment_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ArcAppLaunchThrottleObserver app_launch_observer_;
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
  observer()->OnTaskCreated(0, app2.package_name, "", "", /*session_id=*/0);
  EXPECT_TRUE(observer()->active());

  // App3 finishes launch but observer is not waiting for app3, so it is still
  // active.
  observer()->OnTaskCreated(0, app3.package_name, "", "", /*session_id=*/0);

  // App1 finishes launch, observer is inactive.
  observer()->OnTaskCreated(0, app1.package_name, "", "", /*session_id=*/0);
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
