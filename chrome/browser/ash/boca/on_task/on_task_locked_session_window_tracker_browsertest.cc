// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using ash::boca::OnTaskSystemWebAppManagerImpl;
using ::testing::IsNull;
using ::testing::NotNull;

namespace {

constexpr char kTabUrl1[] = "http://example.com";
constexpr char kTabUrl2[] = "http://company.org";

class OnTaskLockedSessionWindowTrackerBrowserTest
    : public InProcessBrowserTest {
 protected:
  OnTaskLockedSessionWindowTrackerBrowserTest() {
    // Enable Boca and consumer experience for testing purposes. This is used
    // to set up the Boca SWA for OnTask.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kBoca,
                              ash::features::kBocaConsumer},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    ash::SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();
    system_web_app_manager_ =
        std::make_unique<OnTaskSystemWebAppManagerImpl>(profile());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    system_web_app_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  Browser* FindBocaSystemWebAppBrowser() {
    return ash::FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::BOCA);
  }

  Profile* profile() { return browser()->profile(); }

  OnTaskSystemWebAppManagerImpl* system_web_app_manager() {
    return system_web_app_manager_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<OnTaskSystemWebAppManagerImpl> system_web_app_manager_;
};

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       ClosingAllTabsShouldCloseTheAppWindow) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window.
  const SessionID window_id =
      system_web_app_manager()->GetActiveSystemWebAppWindowID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*active_tab_tracker=*/nullptr);

  // Spawn two tabs for testing purposes (outside the homepage tab).
  system_web_app_manager()->CreateBackgroundTabWithUrl(
      window_id, GURL(kTabUrl1),
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  system_web_app_manager()->CreateBackgroundTabWithUrl(
      window_id, GURL(kTabUrl2),
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 3);

  // Close all tabs and verify that the app window is closed.
  boca_app_browser->tab_strip_model()->CloseAllTabs();
  content::RunAllTasksUntilIdle();
  EXPECT_THAT(FindBocaSystemWebAppBrowser(), IsNull());
}

}  // namespace
