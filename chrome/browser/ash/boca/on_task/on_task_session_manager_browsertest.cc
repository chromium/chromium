// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ui/wm/window_util.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::boca::LockedNavigationOptions;
using ::testing::IsNull;
using ::testing::NotNull;

namespace ash::boca {
namespace {

constexpr char kSessionId[] = "test_session_id";
constexpr char kSessionId2[] = "test_session_id_2";
constexpr char kTestUrl1[] = "https://test1.com";
constexpr char kTestUrl2[] = "https://test2.com";

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
    host_resolver()->AddRule("*", "127.0.0.1");
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_server()->AddDefaultHandlers(
        InProcessBrowserTest::GetChromeTestDataDir());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  OnTaskSessionManager* GetOnTaskSessionManager() {
    ash::BocaManager* const boca_manager =
        ash::BocaManagerFactory::GetInstance()->GetForProfile(profile());
    return boca_manager->GetOnTaskSessionManager();
  }

  Browser* FindBocaSystemWebAppBrowser() {
    return ash::FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::BOCA);
  }

  Profile* profile() { return browser()->profile(); }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldOpenTabsOnBundleUpdated) {
  content::TestNavigationObserver navigation_observer_1((GURL(kTestUrl1)));
  navigation_observer_1.StartWatchingNewWebContents();
  content::TestNavigationObserver navigation_observer_2((GURL(kTestUrl2)));
  navigation_observer_2.StartWatchingNewWebContents();

  // Start OnTask session and spawn two tabs outside the homepage tab.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  navigation_observer_1.Wait();
  navigation_observer_2.Wait();

  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 3);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl1));
  tab_strip_model->ActivateTabAt(2);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl2));
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldAddTabsWhenAdditionalTabsFoundInBundle) {
  content::TestNavigationObserver navigation_observer((GURL(kTestUrl1)));
  navigation_observer.StartWatchingNewWebContents();

  // Start OnTask session and spawn one tab outside the homepage tab.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  navigation_observer.Wait();

  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl1));

  // Add another tab to the bundle.
  bundle.add_content_configs()->set_url(kTestUrl2);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_EQ(tab_strip_model->count(), 3);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            GURL(kTestUrl1));
  tab_strip_model->ActivateTabAt(2);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            GURL(kTestUrl2));
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldRemoveTabsWhenFewerTabsFoundInBundle) {
  content::TestNavigationObserver navigation_observer_1((GURL(kTestUrl1)));
  navigation_observer_1.StartWatchingNewWebContents();
  content::TestNavigationObserver navigation_observer_2((GURL(kTestUrl2)));
  navigation_observer_2.StartWatchingNewWebContents();

  // Start OnTask session and spawn two tabs outside the homepage tab.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  navigation_observer_1.Wait();
  navigation_observer_2.Wait();

  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 3);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl1));
  tab_strip_model->ActivateTabAt(2);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl2));

  // Remove one tab from the bundle.
  bundle.clear_content_configs();
  bundle.add_content_configs()->set_url(kTestUrl1);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            GURL(kTestUrl1));
}

IN_PROC_BROWSER_TEST_F(
    OnTaskSessionManagerBrowserTest,
    ShouldPinAndUnpinBocaSWAWhenLockAndUnlockOnBundleUpdated) {
  content::TestNavigationObserver navigation_observer((GURL(kTestUrl1)));
  navigation_observer.StartWatchingNewWebContents();

  // Start OnTask session and spawn one tab outside the homepage tab.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  navigation_observer.Wait();

  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Lock the boca app.
  bundle.set_locked(true);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_TRUE(boca_app_browser->window()->IsToolbarVisible());

  // Unlock the boca app.
  bundle.set_locked(false);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_TRUE(boca_app_browser->window()->IsToolbarVisible());

  // Attempt to lock the boca app again to simulate real world scenario.
  bundle.set_locked(true);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_TRUE(boca_app_browser->window()->IsToolbarVisible());

  // Unlock the Boca app to unblock test teardown that involves browser window
  // close.
  bundle.set_locked(false);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  EXPECT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldApplyOpenNavRestrictionsToTabsOnBundleUpdated) {
  content::TestNavigationObserver navigation_observer((GURL(kTestUrl1)));
  navigation_observer.StartWatchingNewWebContents();

  // Start OnTask session and spawn one tab outside the homepage tab.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  ::boca::Bundle bundle;
  ::boca::ContentConfig* const content_config =
      bundle.mutable_content_configs()->Add();
  content_config->set_url(kTestUrl1);
  // Set the tab navigation type to be 'Allow all navigation'.
  content_config->mutable_locked_navigation_options()->set_navigation_type(
      LockedNavigationOptions::OPEN_NAVIGATION);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  navigation_observer.Wait();

  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl1));

  // Navigate the active tab to the new url.
  const GURL new_url(embedded_test_server()->GetURL("/test/new_page.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, new_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            new_url);
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldApplyBlockNavRestrictionsToTabsOnBundleUpdated) {
  content::TestNavigationObserver navigation_observer((GURL(kTestUrl1)));
  navigation_observer.StartWatchingNewWebContents();

  // Start OnTask session and spawn one tab outside the homepage tab.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  ::boca::Bundle bundle;
  ::boca::ContentConfig* const content_config =
      bundle.mutable_content_configs()->Add();
  content_config->set_url(kTestUrl1);
  // Set the tab navigation type to be 'Just this page'.
  content_config->mutable_locked_navigation_options()->set_navigation_type(
      LockedNavigationOptions::BLOCK_NAVIGATION);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  navigation_observer.Wait();

  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl1));

  // Navigate the active tab to the new url.
  const GURL new_url(embedded_test_server()->GetURL("/test/new_page.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, new_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl1));
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldApplyDomainNavRestrictionsToTabsOnBundleUpdated) {
  content::TestNavigationObserver navigation_observer((GURL(kTestUrl1)));
  navigation_observer.StartWatchingNewWebContents();

  // Start OnTask session and spawn one tab outside the homepage tab.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  ::boca::Bundle bundle;
  ::boca::ContentConfig* const content_config =
      bundle.mutable_content_configs()->Add();
  content_config->set_url(kTestUrl1);
  // Set the tab navigation type to be 'Anywhere on this site'.
  content_config->mutable_locked_navigation_options()->set_navigation_type(
      LockedNavigationOptions::DOMAIN_NAVIGATION);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  navigation_observer.Wait();

  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl1));

  // Navigate the active tab to the new url.
  const GURL same_domain_url(
      embedded_test_server()->GetURL("test1.com", "/test/new_page.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, same_domain_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            same_domain_url);
  const GURL new_domain_url(
      embedded_test_server()->GetURL("test2.com", "/test/new_page.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, new_domain_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            same_domain_url);
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldApplyLimitedNavRestrictionsToTabsOnBundleUpdated) {
  content::TestNavigationObserver navigation_observer((GURL(kTestUrl1)));
  navigation_observer.StartWatchingNewWebContents();

  // Start OnTask session and spawn one tab outside the homepage tab.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  ::boca::Bundle bundle;
  ::boca::ContentConfig* const content_config =
      bundle.mutable_content_configs()->Add();
  content_config->set_url(kTestUrl1);
  // Set the tab navigation type to be 'One link away from this page'.
  content_config->mutable_locked_navigation_options()->set_navigation_type(
      LockedNavigationOptions::LIMITED_NAVIGATION);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  navigation_observer.Wait();

  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl1));

  // Navigate the active tab to the new url.
  const GURL one_level_url(
      embedded_test_server()->GetURL("/test/new_page.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, one_level_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            one_level_url);
  const GURL two_level_url(
      embedded_test_server()->GetURL("/test/new_page_2.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, two_level_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            one_level_url);
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldCloseBocaSWAOnSessionEnd) {
  const GURL boca_chrome_url = GURL(kChromeBocaAppUntrustedIndexURL);
  content::TestNavigationObserver navigation_observer(boca_chrome_url);
  navigation_observer.StartWatchingNewWebContents();

  // Start OnTask session.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  navigation_observer.Wait();

  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());
  // End the session.
  GetOnTaskSessionManager()->OnSessionEnded(kSessionId);
  // Wait until the browser actually gets closed.
  ui_test_utils::WaitForBrowserToClose();
  ASSERT_THAT(FindBocaSystemWebAppBrowser(), IsNull());
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       RestoreTabsOnAppReload) {
  content::TestNavigationObserver navigation_observer((GURL(kTestUrl1)));
  navigation_observer.StartWatchingNewWebContents();

  // Start OnTask session and spawn two tabs outside the homepage tab.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  navigation_observer.Wait();

  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 3);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl1));
  tab_strip_model->ActivateTabAt(2);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl2));

  // Attempt an app reload and verify tabs are restored.
  GetOnTaskSessionManager()->OnAppReloaded();
  ASSERT_EQ(tab_strip_model->count(), 3);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            GURL(kTestUrl1));
  tab_strip_model->ActivateTabAt(2);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            GURL(kTestUrl2));
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldOpenBocaSWAOnSessionTakeover) {
  content::TestNavigationObserver navigation_observer((GURL(kTestUrl1)));
  navigation_observer.StartWatchingNewWebContents();

  // Start OnTask session and spawn one tab outside the homepage tab.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  navigation_observer.Wait();

  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl1));

  // End the session and start another session.
  GetOnTaskSessionManager()->OnSessionEnded(kSessionId);
  content::TestNavigationObserver navigation_observer_2((GURL(kTestUrl2)));
  navigation_observer_2.StartWatchingNewWebContents();
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId2,
                                              ::boca::UserIdentity());
  ::boca::Bundle bundle_2;
  bundle_2.add_content_configs()->set_url(kTestUrl2);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle_2);
  navigation_observer_2.Wait();

  Browser* const boca_app_browser_2 = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser_2, NotNull());
  ASSERT_TRUE(boca_app_browser_2->IsLockedForOnTask());
  auto* const tab_strip_model_2 = boca_app_browser_2->tab_strip_model();
  ASSERT_EQ(tab_strip_model_2->count(), 2);
  tab_strip_model_2->ActivateTabAt(1);
  EXPECT_EQ(tab_strip_model_2->GetActiveWebContents()->GetLastCommittedURL(),
            GURL(kTestUrl2));
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldMuteTabsAudioWhenLockOnBundleUpdated) {
  content::TestNavigationObserver navigation_observer((GURL(kTestUrl1)));
  navigation_observer.StartWatchingNewWebContents();

  // Start OnTask session and spawn one tab outside the homepage tab.
  OnTaskSessionManager* const on_task_session_manager =
      GetOnTaskSessionManager();
  on_task_session_manager->OnSessionStarted(kSessionId, ::boca::UserIdentity());
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  on_task_session_manager->OnBundleUpdated(bundle);
  navigation_observer.Wait();

  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Open first browser window.
  Browser* const browser_1 = browser();
  chrome::NewTab(browser_1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser_1, GURL(kTestUrl1)));

  // Open second browser window.
  Browser* const browser_2 =
      Browser::Create(Browser::CreateParams(profile(), true));
  chrome::NewTab(browser_2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser_2, GURL(kTestUrl2)));

  // Lock the boca app and tabs in boca app browser are not muted.
  bundle.set_locked(true);
  on_task_session_manager->OnBundleUpdated(bundle);
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_FALSE(tab_strip_model->GetActiveWebContents()->IsAudioMuted());

  // Tabs in other browsers are muted.
  EXPECT_TRUE(
      browser_1->tab_strip_model()->GetActiveWebContents()->IsAudioMuted());
  EXPECT_TRUE(
      browser_2->tab_strip_model()->GetActiveWebContents()->IsAudioMuted());

  // Unlock the boca app and tabs in boca app browser are not muted.
  bundle.set_locked(false);
  on_task_session_manager->OnBundleUpdated(bundle);
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  EXPECT_FALSE(tab_strip_model->GetActiveWebContents()->IsAudioMuted());

  // Tabs in other browsers are muted.
  EXPECT_TRUE(
      browser_1->tab_strip_model()->GetActiveWebContents()->IsAudioMuted());
  EXPECT_TRUE(
      browser_2->tab_strip_model()->GetActiveWebContents()->IsAudioMuted());
}

}  // namespace
}  // namespace ash::boca
