// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ui/wm/window_util.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::boca::LockedNavigationOptions;
using ::testing::IsNull;
using ::testing::NotNull;

namespace ash::boca {
namespace {

constexpr char kTestUrl1[] = "https://www.test1.com";
constexpr char kTestUrl2[] = "https://www.test2.com";

class OnTaskSessionManagerBrowserTest : public InProcessBrowserTest {
 protected:
  OnTaskSessionManagerBrowserTest() {
    // Enable Boca and consumer experience for testing purposes. This is used
    // to set up the Boca SWA for OnTask.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kBoca, features::kBocaConsumer},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    ash::SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();
    ASSERT_TRUE(embedded_test_server()->Start());
    system_web_app_manager_ =
        std::make_unique<OnTaskSystemWebAppManagerImpl>(profile());
    // Set up OnTask session for testing purposes.
    GetOnTaskSessionManager()->OnSessionStarted("test_session",
                                                ::boca::UserIdentity());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    system_web_app_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  OnTaskSessionManager* GetOnTaskSessionManager() {
    ash::BocaManager* const boca_manager =
        ash::BocaManagerFactory::GetInstance()->GetForProfile(profile());
    return boca_manager->GetOnTaskSessionManagerForTesting();
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

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldOpenTabsOnBundleUpdated) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->session_id();
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn two tabs outside the homepage tab.
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 3);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl1)));
  tab_strip_model->ActivateTabAt(2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl2)));
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldAddTabsWhenAdditionalTabsFoundInBundle) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->session_id();
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn one tab outside the homepage tab.
  ::boca::Bundle bundle_1;
  bundle_1.add_content_configs()->set_url(kTestUrl1);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle_1);
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl1)));

  // Add one tab to the bundle.
  ::boca::Bundle bundle_2;
  bundle_2.add_content_configs()->set_url(kTestUrl1);
  bundle_2.add_content_configs()->set_url(kTestUrl2);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle_2);
  ASSERT_EQ(tab_strip_model->count(), 3);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl1)));
  tab_strip_model->ActivateTabAt(2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl2)));
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldRemoveTabsWhenFewerTabsFoundInBundle) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->session_id();
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn two tabs outside the homepage tab.
  ::boca::Bundle bundle_1;
  bundle_1.add_content_configs()->set_url(kTestUrl1);
  bundle_1.add_content_configs()->set_url(kTestUrl2);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle_1);
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 3);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl1)));
  tab_strip_model->ActivateTabAt(2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl2)));

  // Remove one tab from the bundle.
  ::boca::Bundle bundle_2;
  bundle_2.add_content_configs()->set_url(kTestUrl1);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle_2);
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl1)));
}

IN_PROC_BROWSER_TEST_F(
    OnTaskSessionManagerBrowserTest,
    ShouldPinAndUnpinBocaSWAWhenLockAndUnlockOnBundleUpdated) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->session_id();
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn one tab outside the homepage tab and lock the boca app.
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.set_locked(true);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  EXPECT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_TRUE(boca_app_browser->window()->IsToolbarVisible());

  // Unlock the boca app.
  bundle.set_locked(false);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  EXPECT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_TRUE(boca_app_browser->window()->IsToolbarVisible());
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldApplyRestrictionsToTabsOnBundleUpdated) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->session_id();
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn one tab outside the homepage tab.
  ::boca::Bundle bundle;
  ::boca::ContentConfig* const content_config =
      bundle.mutable_content_configs()->Add();
  content_config->set_url(kTestUrl1);

  // Set the tab navigation type to be 'Allow all navigation'.
  content_config->mutable_locked_navigation_options()->set_navigation_type(
      LockedNavigationOptions::OPEN_NAVIGATION);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl1)));
  GURL new_url(embedded_test_server()->GetURL("/test/new_page.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, new_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(), new_url);

  // Set the tab navigation type to be 'Just this page'.
  content_config->mutable_locked_navigation_options()->set_navigation_type(
      LockedNavigationOptions::BLOCK_NAVIGATION);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl1)));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, new_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            GURL(kTestUrl1));

  // Set the tab navigation type to be 'Anywhere on this site'.
  content_config->mutable_locked_navigation_options()->set_navigation_type(
      LockedNavigationOptions::DOMAIN_NAVIGATION);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl1)));
  GURL new_domain_url(
      embedded_test_server()->GetURL("test1.com", "/test/new_page.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, new_domain_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            new_domain_url);

  // Set the tab navigation type to be 'One link away from this page'.
  content_config->mutable_locked_navigation_options()->set_navigation_type(
      LockedNavigationOptions::LIMITED_NAVIGATION);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl1)));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, new_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(), new_url);
  GURL new_url_2(embedded_test_server()->GetURL("/test/new_page_2.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, new_url_2));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(), new_url);
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldCloseBocaSWAOnSessionEnd) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // End the session.
  GetOnTaskSessionManager()->OnSessionEnded("test_session");
  ASSERT_THAT(FindBocaSystemWebAppBrowser(), IsNull());
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       RestoreTabsOnAppReload) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->session_id();
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn two tabs outside the homepage tab.
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 3);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl1)));
  tab_strip_model->ActivateTabAt(2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl2)));

  // Attempt an app reload and verify tabs are restored.
  GetOnTaskSessionManager()->OnAppReloaded();
  ASSERT_EQ(tab_strip_model->count(), 3);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl1)));
  tab_strip_model->ActivateTabAt(2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, GURL(kTestUrl2)));
}

}  // namespace
}  // namespace ash::boca
