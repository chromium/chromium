// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

using ash::boca::OnTaskSystemWebAppManagerImpl;
using ::boca::LockedNavigationOptions;
using ::testing::IsNull;
using ::testing::NotNull;

namespace ash::boca {
namespace {

constexpr char kTabUrl1[] = "https://example.com";
constexpr char kTabUrl2[] = "https://company.org";
constexpr char kChromeBocaAppQueryUrl[] =
    "chrome-untrusted://boca-app/q?queryForm";

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

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  void SetUpOnMainThread() override {
    ash::SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();
    system_web_app_manager_ =
        std::make_unique<OnTaskSystemWebAppManagerImpl>(profile());
    host_resolver()->AddRule("*", "127.0.0.1");
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    https_server()->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));

    // Set up OnTask session for testing purposes. Especially needed to ensure
    // newly created tabs are not deleted.
    GetOnTaskSessionManager()->OnSessionStarted("test_session",
                                                ::boca::UserIdentity());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    system_web_app_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  Browser* FindBocaSystemWebAppBrowser() {
    return ash::FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::BOCA);
  }

  OnTaskSessionManager* GetOnTaskSessionManager() {
    ash::BocaManager* const boca_manager =
        ash::BocaManagerFactory::GetInstance()->GetForProfile(profile());
    return boca_manager->GetOnTaskSessionManagerForTesting();
  }

  Profile* profile() { return browser()->profile(); }

  OnTaskSystemWebAppManagerImpl* system_web_app_manager() {
    return system_web_app_manager_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<OnTaskSystemWebAppManagerImpl> system_web_app_manager_;
  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
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
      window_id, /*observers=*/{});

  // Spawn two tabs for testing purposes (outside the homepage tab).
  system_web_app_manager()->CreateBackgroundTabWithUrl(
      window_id, GURL(kTabUrl1), LockedNavigationOptions::OPEN_NAVIGATION);
  system_web_app_manager()->CreateBackgroundTabWithUrl(
      window_id, GURL(kTabUrl2), LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 3);

  // Close all tabs and verify that the app window is closed.
  boca_app_browser->tab_strip_model()->CloseAllTabs();
  content::RunAllTasksUntilIdle();
  EXPECT_THAT(FindBocaSystemWebAppBrowser(), IsNull());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       BlockFileUrlTypes) {
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
      window_id, /*observers=*/{});

  // Spawns a tab for testing purposes (outside the homepage tab).
  const GURL base_url(kTabUrl1);
  system_web_app_manager()->CreateBackgroundTabWithUrl(
      window_id, base_url, LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 2);
  boca_app_browser->tab_strip_model()->ActivateTabAt(1);

  // File urls are blocked.
  const GURL file_url(GURL("file:///foo.com/download.zip"));
  content::TestNavigationObserver url_obs(
      boca_app_browser->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, file_url));
  url_obs.Wait();
  EXPECT_EQ(base_url, boca_app_browser->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());
  EXPECT_FALSE(url_obs.last_navigation_succeeded());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       BlockChromeUrlTypesExceptBocaAppURL) {
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
      window_id, /*observers=*/{});

  // Spawns a tab for testing purposes (outside the homepage tab).
  const GURL base_url(kTabUrl1);
  system_web_app_manager()->CreateBackgroundTabWithUrl(
      window_id, base_url, LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 2);
  boca_app_browser->tab_strip_model()->ActivateTabAt(1);

  // Chrome urls are blocked.
  const GURL chrome_url = GURL(chrome::kChromeUIVersionURL);
  content::TestNavigationObserver url_obs(
      boca_app_browser->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, chrome_url));
  url_obs.Wait();
  EXPECT_EQ(base_url, boca_app_browser->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());
  EXPECT_FALSE(url_obs.last_navigation_succeeded());

  // Boca App chrome url is allowed.
  const GURL boca_chrome_url = GURL(kChromeBocaAppUntrustedIndexURL);
  content::TestNavigationObserver boca_url_obs(
      boca_app_browser->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, boca_chrome_url));
  boca_url_obs.Wait();
  EXPECT_EQ(boca_chrome_url, boca_app_browser->tab_strip_model()
                                 ->GetActiveWebContents()
                                 ->GetLastCommittedURL());
  EXPECT_TRUE(boca_url_obs.last_navigation_succeeded());

  // Boca App chrome url with query is allowed.
  const GURL boca_with_query_chrome_url = GURL(kChromeBocaAppQueryUrl);
  content::TestNavigationObserver boca_query_url_obs(
      boca_app_browser->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser,
                                           boca_with_query_chrome_url));
  boca_query_url_obs.Wait();
  EXPECT_EQ(boca_with_query_chrome_url, boca_app_browser->tab_strip_model()
                                            ->GetActiveWebContents()
                                            ->GetLastCommittedURL());
  EXPECT_TRUE(boca_query_url_obs.last_navigation_succeeded());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       BlockBlobUrlTypes) {
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
      window_id, /*observers=*/{});

  // Spawns a tab for testing purposes (outside the homepage tab).
  const GURL base_url(kTabUrl1);
  system_web_app_manager()->CreateBackgroundTabWithUrl(
      window_id, base_url, LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 2);
  boca_app_browser->tab_strip_model()->ActivateTabAt(1);

  // Blob urls are blocked.
  const GURL blob_url(GURL("blob:https://foo.com/uuid"));
  content::TestNavigationObserver url_obs(
      boca_app_browser->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, blob_url));
  url_obs.Wait();
  EXPECT_EQ(base_url, boca_app_browser->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());
  EXPECT_FALSE(url_obs.last_navigation_succeeded());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       BlockDownloadUrlTypes) {
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
      window_id, /*observers=*/{});

  // Spawns a tab for testing purposes (outside the homepage tab).
  const GURL base_url(kTabUrl1);
  system_web_app_manager()->CreateBackgroundTabWithUrl(
      window_id, base_url, LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 2);
  boca_app_browser->tab_strip_model()->ActivateTabAt(1);

  // Download urls are blocked and setup monitoring of the download.
  const GURL download_url(
      embedded_test_server()->GetURL("foo.com",
                                     "/set-header?Content-Disposition: "
                                     "attachment"));
  content::NavigationHandleObserver nav_handle_obs(
      boca_app_browser->tab_strip_model()->GetActiveWebContents(),
      download_url);
  content::TestNavigationObserver url_obs(
      boca_app_browser->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, download_url));
  EXPECT_FALSE(nav_handle_obs.has_committed());
  EXPECT_TRUE(nav_handle_obs.is_download());
  EXPECT_FALSE(url_obs.last_navigation_succeeded());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       AllowOpeningInNewWindow) {
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
      window_id, /*observers=*/{});

  // Spawns a tab for testing purposes (outside the homepage tab).
  const GURL base_url(kTabUrl1);
  system_web_app_manager()->CreateBackgroundTabWithUrl(
      window_id, base_url, LockedNavigationOptions::BLOCK_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 2);
  boca_app_browser->tab_strip_model()->ActivateTabAt(1);

  // Open the url in a new tab.
  const GURL url_for_new_window = GURL(kTabUrl2);
  content::ContextMenuParams params;
  params.is_editable = false;
  params.page_url = base_url;
  params.link_url = url_for_new_window;

  content::WebContents* web_contents =
      boca_app_browser->tab_strip_model()->GetActiveWebContents();
  TestRenderViewContextMenu menu(*web_contents->GetPrimaryMainFrame(), params);
  ui_test_utils::TabAddedWaiter tab_add(browser());

  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  tab_add.Wait();
  int index_of_new_tab = browser()->tab_strip_model()->count() - 1;
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(index_of_new_tab);

  // Verify that the URL was committed.
  content::NavigationController& navigation_controller =
      new_web_contents->GetController();
  WaitForLoadStop(new_web_contents);
  EXPECT_FALSE(navigation_controller.GetLastCommittedEntry()->IsInitialEntry());
  EXPECT_EQ(url_for_new_window, new_web_contents->GetLastCommittedURL());
  EXPECT_EQ(base_url, boca_app_browser->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());
}

}  // namespace
}  // namespace ash::boca
