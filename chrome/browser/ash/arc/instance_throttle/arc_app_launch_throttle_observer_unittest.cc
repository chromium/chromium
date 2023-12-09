// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_app_launch_throttle_observer.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/mojom/compatibility_mode.mojom.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/idle_manager/arc_throttle_test_observer.h"
#include "chrome/browser/ash/arc/util/arc_window_watcher.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

ArcAppListPrefs::AppInfo CreateAppInfoForPackage(
    const std::string package_name) {
  return ArcAppListPrefs::AppInfo(
      package_name, package_name, "" /* activity */, "" /* intent_uri */,
      "" /* icon_resource_id */, std::nullopt /* version_name */,
      base::Time() /* last_launch_time */, base::Time() /* install_time */,
      true /* sticky */, true /* notifications_enabled */,
      arc::mojom::ArcResizeLockState::UNDEFINED,
      true /* resize_lock_needs_confirmation */,
      ArcAppListPrefs::WindowLayout(), true /* ready */, true /* suspended */,
      true /* show_in_launcher */, true /* shortcut */, true /* launchable */,
      false /* need_fixup */, std::nullopt /* app_size_in_bytes */,
      std::nullopt /* data_size_in_bytes */,
      arc::mojom::AppCategory::kUndefined);
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
  ash::ArcWindowWatcher* watcher() { return ash::ArcWindowWatcher::instance(); }

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
  observer()->OnArcAppLaunchRequested(app1.package_name);
  EXPECT_TRUE(observer()->active());

  // App2 launch requested but finishes before App1, observer is still active.
  observer()->OnArcAppLaunchRequested(app2.package_name);
  observer()->OnTaskCreated(0, app2.package_name, "", "", /*session_id=*/0);
  EXPECT_TRUE(observer()->active());

  // App3 finishes launch but observer is not waiting for app3, so it is still
  // active.
  observer()->OnTaskCreated(0, app3.package_name, "", "", /*session_id=*/0);
  EXPECT_TRUE(observer()->active());

  // App1 finishes launch, observer is inactive.
  observer()->OnTaskCreated(0, app1.package_name, "", "", /*session_id=*/0);
  EXPECT_FALSE(observer()->active());

  // Re-launch App1, observer is active
  observer()->OnArcAppLaunchRequested(app1.package_name);
  EXPECT_TRUE(observer()->active());

  // App1 finishes launch, observer is inactive.
  observer()->OnArcWindowDisplayed(app1.package_name);
  EXPECT_FALSE(observer()->active());
}

// Check that a launch request expires.
TEST_F(ArcAppLaunchThrottleObserverTest, TestLaunchRequestExpires) {
  const ArcAppListPrefs::AppInfo app =
      CreateAppInfoForPackage("com.android.app");
  EXPECT_FALSE(observer()->active());

  observer()->OnArcAppLaunchRequested(app.package_name);
  EXPECT_TRUE(observer()->active());

  environment()->FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(observer()->active());
}

TEST_F(ArcAppLaunchThrottleObserverTest, TestWindowWatcherEffectOnInit) {
  std::unique_ptr<TestingProfile> testing_profile;

  testing_profile = std::make_unique<TestingProfile>();

  {  // ArcWindowWatcher available.
    std::unique_ptr<ash::ArcWindowWatcher> arc_window_watcher;
    arc_window_watcher = std::make_unique<ash::ArcWindowWatcher>();

    unittest::ThrottleTestObserver test_observer;

    EXPECT_EQ(0, test_observer.count());
    EXPECT_FALSE(watcher()->HasDisplayObserver(observer()));
    observer()->StartObserving(
        testing_profile.get(),
        base::BindRepeating(&unittest::ThrottleTestObserver::Monitor,
                            base::Unretained(&test_observer)));
    EXPECT_TRUE(watcher()->HasDisplayObserver(observer()));

    observer()->StopObserving();
    EXPECT_FALSE(watcher()->HasDisplayObserver(observer()));
  }

  {  // ArcWindowWatcher unavailable.
    unittest::ThrottleTestObserver test_observer;

    EXPECT_EQ(0, test_observer.count());
    observer()->StartObserving(
        testing_profile.get(),
        base::BindRepeating(&unittest::ThrottleTestObserver::Monitor,
                            base::Unretained(&test_observer)));
    // No need to check: not crashing is enough to prove correctness.

    observer()->StopObserving();
  }
}

TEST_F(ArcAppLaunchThrottleObserverTest, TestCallbackString) {
  auto temp_id = std::make_unique<std::string>("test_app_id");
  observer()->OnArcAppLaunchRequested(*temp_id);
  temp_id.reset();

  // Expect no crash & no memory leak on asan/lsan test.
  environment()->FastForwardUntilNoTasksRemain();
}

}  // namespace
}  // namespace arc
