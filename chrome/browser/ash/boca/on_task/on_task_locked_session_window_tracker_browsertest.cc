// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/boca/boca_window_observer.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using ash::boca::OnTaskSystemWebAppManagerImpl;
using ::boca::LockedNavigationOptions;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::IsNull;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Sequence;

namespace ash::boca {
namespace {

constexpr char kTabUrl1Host[] = "www.example.com";
constexpr char kTabUrl1SubDomainHost[] = "example.child.com";
constexpr char kTabUrl1FrontSubDomainHost[] = "sub.example.com";
constexpr char kTabUrl2Host[] = "www.company.org";
constexpr char kTabGoogleHost[] = "www.google.com";
constexpr char kTabGoogleDocsHost[] = "docs.google.com";
constexpr char kChromeBocaAppQueryUrl[] =
    "chrome-untrusted://boca-app/q?queryForm";

// Mock implementation of the `BocaWindowObserver`.
class MockBocaWindowObserver : public ash::boca::BocaWindowObserver {
 public:
  MOCK_METHOD(void,
              OnActiveTabChanged,
              (const std::u16string& title),
              (override));
  MOCK_METHOD(void,
              OnTabAdded,
              (SessionID active_tab_id, SessionID tab_id, GURL url),
              (override));
  MOCK_METHOD(void, OnTabRemoved, (SessionID tab_id), (override));
  MOCK_METHOD(void, OnWindowTrackerCleanedup, (), (override));
};

class OnTaskLockedSessionWindowTrackerBrowserTestBase
    : public InProcessBrowserTest {
 protected:
  OnTaskLockedSessionWindowTrackerBrowserTestBase() {
    // Enable Boca, Boca consumer, and Boca Pod experience for testing purposes.
    // This is used to set up the Boca SWA for OnTask.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kBoca,
                              ash::features::kBocaConsumer,
                              ash::features::kBocaOnTaskPod,
                              ash::features::kOnDeviceSpeechRecognition},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    ash::SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();
    system_web_app_manager_ =
        std::make_unique<OnTaskSystemWebAppManagerImpl>(profile());
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    system_web_app_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Creates a new background tab with the specified url and navigation
  // restrictions, and waits until the specified url has been loaded.
  // Returns the newly created tab id.
  SessionID CreateBackgroundTabAndWait(
      SessionID window_id,
      const GURL& url,
      LockedNavigationOptions::NavigationType restriction_level) {
    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.StartWatchingNewWebContents();
    const SessionID tab_id =
        system_web_app_manager()->CreateBackgroundTabWithUrl(window_id, url,
                                                             restriction_level);
    navigation_observer.Wait();
    return tab_id;
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

class OnTaskLockedSessionWindowTrackerBrowserTest
    : public OnTaskLockedSessionWindowTrackerBrowserTestBase {
 protected:
  void SetUpOnMainThread() override {
    OnTaskLockedSessionWindowTrackerBrowserTestBase::SetUpOnMainThread();
    embedded_test_server()->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SpawnChildTabWithURL(const Browser* browser, const GURL& url) {
    content::WebContents* const active_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver navigation_observer(active_web_contents);
    navigation_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(
        content::ExecJs(active_web_contents,
                        content::JsReplace("window.open($1, '_blank');", url)));
    navigation_observer.WaitForNavigationFinished();
  }
};

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       RegisteringUrlsAndRestrictionLevels) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tabs and verify appropriate restriction levels are set.
  const SessionID tab_id_1 = CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl1Host, "/"),
      LockedNavigationOptions::LIMITED_NAVIGATION);
  const SessionID tab_id_2 = CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl2Host, "/"),
      LockedNavigationOptions::BLOCK_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 3);

  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id_1],
            LockedNavigationOptions::LIMITED_NAVIGATION);
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id_2],
            LockedNavigationOptions::BLOCK_NAVIGATION);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       RegisterChildUrlWithRestrictions) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn child tab and verify appropriate restriction level is set.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL parent_tab_url =
      embedded_test_server()->GetURL(kTabUrl1Host, "/title1.html");
  const SessionID tab_id = CreateBackgroundTabAndWait(
      window_id, parent_tab_url, LockedNavigationOptions::DOMAIN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  SpawnChildTabWithURL(boca_app_browser, embedded_test_server()->GetURL(
                                             kTabUrl1Host, "/title2.html"));
  ASSERT_EQ(tab_strip_model->count(), 3);
  const SessionID child_tab_id = sessions::SessionTabHelper::IdForTab(
      tab_strip_model->GetActiveWebContents());
  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id],
            LockedNavigationOptions::DOMAIN_NAVIGATION);
  EXPECT_EQ(on_task_blocklist->child_tab_to_nav_filters()[child_tab_id],
            LockedNavigationOptions::DOMAIN_NAVIGATION);
}

IN_PROC_BROWSER_TEST_F(
    OnTaskLockedSessionWindowTrackerBrowserTest,
    NavigateCurrentTabWithMultipleRestrictionsMaintainTabRestrictions) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tabs outside the homepage one for testing purposes.
  const GURL url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  const GURL url_subdomain =
      embedded_test_server()->GetURL(kTabUrl1SubDomainHost, "/");
  const SessionID tab_id_1 = CreateBackgroundTabAndWait(
      window_id, url, LockedNavigationOptions::OPEN_NAVIGATION);
  const SessionID tab_id_2 = CreateBackgroundTabAndWait(
      window_id, url_subdomain, LockedNavigationOptions::BLOCK_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 3);
  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id_1],
            LockedNavigationOptions::OPEN_NAVIGATION);
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id_2],
            LockedNavigationOptions::BLOCK_NAVIGATION);

  // Navigate to a known URL that has a different restriction applied and verify
  // the current restriction remains unchanged.
  tab_strip_model->ActivateTabAt(1);
  const LockedNavigationOptions::NavigationType original_restriction_level =
      on_task_blocklist->current_page_restriction_level();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, url_subdomain));
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            original_restriction_level);

  const GURL url_with_query =
      embedded_test_server()->GetURL(kTabUrl1Host, "/q?randomness");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, url_with_query));
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            original_restriction_level);

  const GURL url_with_path =
      embedded_test_server()->GetURL(kTabUrl1Host, "/random/path");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, url_with_path));
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            original_restriction_level);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       SwitchingTabsWithDifferentRestrictions) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tabs outside the homepage one for testing purposes.
  const SessionID tab_id_1 = CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl1Host, "/"),
      LockedNavigationOptions::LIMITED_NAVIGATION);
  const SessionID tab_id_2 = CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl2Host, "/"),
      LockedNavigationOptions::BLOCK_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 3);
  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id_1],
            LockedNavigationOptions::LIMITED_NAVIGATION);
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id_2],
            LockedNavigationOptions::BLOCK_NAVIGATION);

  // Switch between tabs and verify relevant restrictions are applied.
  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            LockedNavigationOptions::LIMITED_NAVIGATION);
  tab_strip_model->ActivateTabAt(2);
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            LockedNavigationOptions::BLOCK_NAVIGATION);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       NavigateCurrentTabFromRedirectUrl) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn a tab outside the homepage one for testing purposes.
  const SessionID tab_id = CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl1Host, "/"),
      LockedNavigationOptions::OPEN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 2);
  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id],
            LockedNavigationOptions::OPEN_NAVIGATION);

  // Simulate a redirect and verify current page restriction level remains
  // unchanged.
  tab_strip_model->ActivateTabAt(1);
  const GURL redirected_url =
      embedded_test_server()->GetURL("redirect-url.com", "/q?randomness");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, redirected_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            redirected_url);
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            LockedNavigationOptions::OPEN_NAVIGATION);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       NavigateCurrentTabThatSpawnsNewTab) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn child tab and verify appropriate restriction level is set.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const SessionID tab_id = CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl1Host, "/title1.html"),
      LockedNavigationOptions::LIMITED_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  SpawnChildTabWithURL(boca_app_browser, embedded_test_server()->GetURL(
                                             kTabUrl1Host, "/title2.html"));
  ASSERT_EQ(tab_strip_model->count(), 3);
  const SessionID child_tab_id = sessions::SessionTabHelper::IdForTab(
      tab_strip_model->GetActiveWebContents());
  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id],
            LockedNavigationOptions::LIMITED_NAVIGATION);
  EXPECT_EQ(on_task_blocklist->child_tab_to_nav_filters()[child_tab_id],
            LockedNavigationOptions::BLOCK_NAVIGATION);
  EXPECT_FALSE(
      on_task_blocklist->IsParentTab(tab_strip_model->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(
    OnTaskLockedSessionWindowTrackerBrowserTest,
    NavigateCurrentTabWithSameDomainAndOneLevelDeepFromRedirectUrl) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL parent_tab_url =
      embedded_test_server()->GetURL(kTabUrl1Host, "/title1.html");
  const SessionID tab_id = CreateBackgroundTabAndWait(
      window_id, parent_tab_url,
      LockedNavigationOptions::
          SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  ASSERT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id],
            LockedNavigationOptions::
                SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION);

  // Simulate redirect within the same domain that opens in a new tab.
  tab_strip_model->ActivateTabAt(1);
  SpawnChildTabWithURL(boca_app_browser, embedded_test_server()->GetURL(
                                             kTabUrl1Host, "/title2.html"));
  ASSERT_EQ(tab_strip_model->count(), 3);
  const SessionID same_domain_redirect_tab_id =
      sessions::SessionTabHelper::IdForTab(
          tab_strip_model->GetActiveWebContents());
  EXPECT_EQ(on_task_blocklist
                ->child_tab_to_nav_filters()[same_domain_redirect_tab_id],
            LockedNavigationOptions::
                SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION);

  // Simulate redirect within a different domain that opens in a new tab.
  tab_strip_model->ActivateTabAt(1);
  SpawnChildTabWithURL(boca_app_browser, embedded_test_server()->GetURL(
                                             kTabUrl2Host, "/title1.html"));
  ASSERT_EQ(tab_strip_model->count(), 4);
  const SessionID different_domain_redirect_tab_id =
      sessions::SessionTabHelper::IdForTab(
          tab_strip_model->GetActiveWebContents());
  EXPECT_EQ(on_task_blocklist
                ->child_tab_to_nav_filters()[different_domain_redirect_tab_id],
            LockedNavigationOptions::BLOCK_NAVIGATION);

  // Simulate redirect within a different domain on the parent tab.
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      boca_app_browser,
      embedded_test_server()->GetURL(kTabUrl2Host, "/title2.html")));
  ASSERT_EQ(tab_strip_model->count(), 4);
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            parent_tab_url);
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            LockedNavigationOptions::
                SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       BlockUrlForBlockNavRestriction) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  const GURL url_1 = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const SessionID tab_id = CreateBackgroundTabAndWait(
      window_id, url_1, LockedNavigationOptions::BLOCK_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id],
            LockedNavigationOptions::BLOCK_NAVIGATION);

  // Manually refresh blocklist to override URL blocklist manager sources.
  tab_strip_model->ActivateTabAt(1);
  on_task_blocklist->RefreshForUrlBlocklist(
      tab_strip_model->GetActiveWebContents());
  content::RunAllTasksUntilIdle();

  // Verify blocklist blocks all other URL navigation.
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            LockedNavigationOptions::BLOCK_NAVIGATION);
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_1),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL url_2 = embedded_test_server()->GetURL(kTabUrl2Host, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_2),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
  const GURL url_1_with_sub_page =
      embedded_test_server()->GetURL(kTabUrl1Host, "/sub-page");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_1_with_sub_page),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       AllowAndBlockUrlForSameDomainNavRestriction) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const SessionID tab_id = CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl1Host, "/"),
      LockedNavigationOptions::DOMAIN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id],
            LockedNavigationOptions::DOMAIN_NAVIGATION);

  // Manually refresh blocklist to override URL blocklist manager sources.
  tab_strip_model->ActivateTabAt(1);
  on_task_blocklist->RefreshForUrlBlocklist(
      tab_strip_model->GetActiveWebContents());
  content::RunAllTasksUntilIdle();

  // Verify blocklist result with other URLs.
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            LockedNavigationOptions::DOMAIN_NAVIGATION);
  const GURL url_1_front_subdomain =
      embedded_test_server()->GetURL(kTabUrl1FrontSubDomainHost, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_1_front_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL url_1_with_sub_page =
      embedded_test_server()->GetURL(kTabUrl1Host, "/sub-page");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_1_with_sub_page),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL url_1_subdomain =
      embedded_test_server()->GetURL(kTabUrl1SubDomainHost, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_1_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
  const GURL url_1_subdomain_page =
      embedded_test_server()->GetURL(kTabUrl1SubDomainHost, "/sub-page");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_1_subdomain_page),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
  const GURL url_2 = embedded_test_server()->GetURL(kTabUrl2Host, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_2),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
  const GURL google_docs_url =
      embedded_test_server()->GetURL(kTabGoogleDocsHost, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(google_docs_url),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       AllowUrlsForOpenNavRestriction) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const SessionID tab_id = CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl1Host, "/"),
      LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id],
            LockedNavigationOptions::OPEN_NAVIGATION);

  // Manually refresh blocklist to override URL blocklist manager sources.
  tab_strip_model->ActivateTabAt(1);
  on_task_blocklist->RefreshForUrlBlocklist(
      tab_strip_model->GetActiveWebContents());
  content::RunAllTasksUntilIdle();

  // Verify blocklist result with other URLs.
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            LockedNavigationOptions::OPEN_NAVIGATION);
  const GURL url_1_front_subdomain =
      embedded_test_server()->GetURL(kTabUrl1FrontSubDomainHost, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_1_front_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL url_1_with_sub_page =
      embedded_test_server()->GetURL(kTabUrl1Host, "/sub-page");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_1_with_sub_page),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL url_1_subdomain =
      embedded_test_server()->GetURL(kTabUrl1SubDomainHost, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_1_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL url_1_subdomain_page =
      embedded_test_server()->GetURL(kTabUrl1SubDomainHost, "/sub-page");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_1_subdomain_page),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL url_2 = embedded_test_server()->GetURL(kTabUrl2Host, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_2),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       AllowAndBlockUrlForGoogleSameDomainNavRestriction) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const SessionID tab_id = CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabGoogleHost, "/"),
      LockedNavigationOptions::DOMAIN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id],
            LockedNavigationOptions::DOMAIN_NAVIGATION);

  // Manually refresh blocklist to override URL blocklist manager sources.
  tab_strip_model->ActivateTabAt(1);
  on_task_blocklist->RefreshForUrlBlocklist(
      tab_strip_model->GetActiveWebContents());
  content::RunAllTasksUntilIdle();

  // Verify blocklist result with other URLs.
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            LockedNavigationOptions::DOMAIN_NAVIGATION);
  const GURL google_docs_url =
      embedded_test_server()->GetURL(kTabGoogleDocsHost, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(google_docs_url),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL random_google_url =
      embedded_test_server()->GetURL(kTabGoogleHost, "/sub-page");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(random_google_url),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL google_search_url =
      embedded_test_server()->GetURL(kTabGoogleHost, "/?q=test");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(google_search_url),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL url_1_subdomain =
      embedded_test_server()->GetURL(kTabUrl1SubDomainHost, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_1_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
  const GURL url_2 = embedded_test_server()->GetURL(kTabUrl2Host, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_2),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       AllowAndBlockUrlForWorkspaceNavRestriction) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const SessionID tab_id = CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl1Host, "/"),
      LockedNavigationOptions::WORKSPACE_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id],
            LockedNavigationOptions::WORKSPACE_NAVIGATION);

  // Manually refresh blocklist to override URL blocklist manager sources.
  tab_strip_model->ActivateTabAt(1);
  on_task_blocklist->RefreshForUrlBlocklist(
      tab_strip_model->GetActiveWebContents());
  content::RunAllTasksUntilIdle();

  // Verify blocklist result with other URLs.
  EXPECT_EQ(on_task_blocklist->current_page_restriction_level(),
            LockedNavigationOptions::WORKSPACE_NAVIGATION);
  const GURL google_docs_url =
      embedded_test_server()->GetURL(kTabGoogleDocsHost, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(google_docs_url),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL random_google_url =
      embedded_test_server()->GetURL(kTabGoogleHost, "/sub-page");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(random_google_url),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL google_search_url =
      embedded_test_server()->GetURL(kTabGoogleHost, "/search?q=test");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(google_search_url),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL google_redirect_url = embedded_test_server()->GetURL(
      kTabGoogleHost, "/url?q=https://classroom.google.com");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(google_redirect_url),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  const GURL url_1_subdomain =
      embedded_test_server()->GetURL(kTabUrl1SubDomainHost, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_1_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
  const GURL url_2 = embedded_test_server()->GetURL(kTabUrl2Host, "/");
  EXPECT_EQ(on_task_blocklist->GetURLBlocklistState(url_2),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       NewBrowserWindowsDontOpenInLockedFullscreen) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Pin window for testing purposes.
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  // Attempt to create a new browser window and verify it gets closed.
  size_t original_browser_count = BrowserList::GetInstance()->size();
  const base::WeakPtr<Browser> browser_weak_ptr =
      Browser::Create(Browser::CreateParams(profile(), /*user_gesture=*/true))
          ->AsWeakPtr();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(BrowserList::GetInstance()->size(), original_browser_count);
  EXPECT_FALSE(browser_weak_ptr);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       NewBrowserWindowsCanBeSpawnedInUnlockedMode) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  // Attempt to create a new browser window and verify it is closed.
  size_t original_browser_count = BrowserList::GetInstance()->size();
  CreateBrowser(profile());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(BrowserList::GetInstance()->size(), original_browser_count + 1);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       NewPopupIsRegistered) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  // Attempt to create a new popup and verify window tracker picks it up.
  size_t original_browser_count = BrowserList::GetInstance()->size();
  Browser* const popup_browser = Browser::Create(Browser::CreateParams(
      Browser::TYPE_APP_POPUP, profile(), /*user_gesture=*/true));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(BrowserList::GetInstance()->size(), original_browser_count + 1);
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());
  EXPECT_FALSE(window_tracker->CanOpenNewPopup());
  popup_browser->window()->Close();
  EXPECT_TRUE(window_tracker->CanOpenNewPopup());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       StopTrackingAppInstanceOnAppClose) {
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
  ASSERT_TRUE(window_id.is_valid());
  MockBocaWindowObserver window_observer;
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{&window_observer});
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  // The first one triggered by boca no longer set active. The second triggered
  // due to browser closing.
  EXPECT_CALL(
      window_observer,
      OnActiveTabChanged(l10n_util::GetStringUTF16(IDS_NOT_IN_CLASS_TOOLS)))
      .Times(2);
  EXPECT_CALL(window_observer, OnWindowTrackerCleanedup).Times(1);

  // Close the app and verify the window tracker stops tracking it.
  boca_app_browser->window()->Close();
  content::RunAllTasksUntilIdle();

  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());

  EXPECT_THAT(window_tracker->browser(), IsNull());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       BrowserTrackingOverride) {
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
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());
  EXPECT_EQ(window_tracker->browser(), boca_app_browser);

  // Override the window tracker to track a different browser instance.
  window_tracker->InitializeBrowserInfoForTracking(browser());
  EXPECT_EQ(window_tracker->browser(), browser());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       NotifyObserverOnTabAdditionsAndDeletions) {
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
  ASSERT_TRUE(window_id.is_valid());
  MockBocaWindowObserver window_observer;
  EXPECT_CALL(window_observer, OnActiveTabChanged(_)).Times(AnyNumber());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{&window_observer});

  // Verify observer is notified on tab addition and removal.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL parent_tab_url =
      embedded_test_server()->GetURL(kTabUrl1Host, "/title1.html");
  Sequence s;
  EXPECT_CALL(window_observer,
              OnTabAdded(SessionID::InvalidValue(), _, _))
      .Times(1)
      .InSequence(s);
  const SessionID parent_tab_id = CreateBackgroundTabAndWait(
      window_id, parent_tab_url, LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);

  // Spawn new child tab after switching to the parent tab and delete it.
  tab_strip_model->ActivateTabAt(1);
  const GURL child_tab_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/title2.html");
  EXPECT_CALL(window_observer, OnTabAdded(parent_tab_id, _, Ne(parent_tab_url)))
      .Times(1)
      .InSequence(s);
  SpawnChildTabWithURL(boca_app_browser, child_tab_url);
  ASSERT_EQ(tab_strip_model->count(), 3);
  const SessionID child_tab_id = sessions::SessionTabHelper::IdForTab(
      tab_strip_model->GetActiveWebContents());
  EXPECT_CALL(window_observer, OnTabRemoved(child_tab_id))
      .Times(1)
      .InSequence(s);
  tab_strip_model->DetachAndDeleteWebContentsAt(2);
  EXPECT_EQ(tab_strip_model->count(), 2);

  // Unregister window observer before it is destructed to prevent UAF errors.
  testing::Mock::VerifyAndClearExpectations(&window_observer);
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());
  window_tracker->RemoveObserver(&window_observer);
}

IN_PROC_BROWSER_TEST_F(
    OnTaskLockedSessionWindowTrackerBrowserTest,
    NotifyObserverForActiveTabChangeWhenSwitchInAndOutOnTask) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  NiceMock<MockBocaWindowObserver> window_observer;
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{&window_observer});

  // Switch out of boca SWA
  EXPECT_CALL(
      window_observer,
      OnActiveTabChanged(l10n_util::GetStringUTF16(IDS_NOT_IN_CLASS_TOOLS)))
      .Times(1);

  BrowserList::GetInstance()->SetLastActive(browser());
  testing::Mock::VerifyAndClearExpectations(&window_observer);

  // Switch back to Boca SWA
  EXPECT_CALL(window_observer, OnActiveTabChanged(_)).Times(1);
  BrowserList::GetInstance()->SetLastActive(boca_app_browser);

  testing::Mock::VerifyAndClearExpectations(&window_observer);
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());
  window_tracker->RemoveObserver(&window_observer);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       NotifyObserverOnActiveTabUpdates) {
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
  ASSERT_TRUE(window_id.is_valid());
  NiceMock<MockBocaWindowObserver> window_observer;
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{&window_observer});

  // Verify observer is notified on tab switch.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL parent_tab_url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(window_id,
                             embedded_test_server()->GetURL(kTabUrl1Host, "/"),
                             LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  EXPECT_CALL(window_observer, OnActiveTabChanged(_)).Times(AtLeast(1));
  tab_strip_model->ActivateTabAt(1);

  // Unregister window observer before it is destructed to prevent UAF errors.
  testing::Mock::VerifyAndClearExpectations(&window_observer);
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());
  window_tracker->RemoveObserver(&window_observer);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       NotifyObserverOnWindowTrackerCleanup) {
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
  ASSERT_TRUE(window_id.is_valid());
  NiceMock<MockBocaWindowObserver> window_observer;
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{&window_observer});

  // Cleanup window tracker and verify observer is notified.
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());
  EXPECT_CALL(window_observer, OnWindowTrackerCleanedup).Times(1);
  window_tracker->InitializeBrowserInfoForTracking(nullptr);

  // Unregister window observer before it is destructed to prevent UAF errors.
  testing::Mock::VerifyAndClearExpectations(&window_observer);
  window_tracker->RemoveObserver(&window_observer);
}

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
  CreateBackgroundTabAndWait(window_id,
                             embedded_test_server()->GetURL(kTabUrl1Host, "/"),
                             LockedNavigationOptions::OPEN_NAVIGATION);
  CreateBackgroundTabAndWait(window_id,
                             embedded_test_server()->GetURL(kTabUrl2Host, "/"),
                             LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 3);

  // Close all tabs and verify that the app window is closed.
  boca_app_browser->tab_strip_model()->CloseAllTabs();
  content::RunAllTasksUntilIdle();
  EXPECT_THAT(FindBocaSystemWebAppBrowser(), IsNull());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       ClosingTheOnlyTabShouldCloseTheAppWindow) {
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
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 1);

  // Close the only tab and verify that the app window is closed.
  boca_app_browser->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);
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
  const GURL base_url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(window_id, base_url,
                             LockedNavigationOptions::OPEN_NAVIGATION);
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
  const GURL base_url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(window_id, base_url,
                             LockedNavigationOptions::OPEN_NAVIGATION);
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
  const GURL base_url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(window_id, base_url,
                             LockedNavigationOptions::OPEN_NAVIGATION);
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
  const GURL base_url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(window_id, base_url,
                             LockedNavigationOptions::BLOCK_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 2);
  boca_app_browser->tab_strip_model()->ActivateTabAt(1);

  // Open the url in a new tab.
  const GURL url_for_new_window =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
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

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       ImmersiveModeRemainsDisabledWhenPaused) {
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

  // Spawn a tab for testing purposes (outside the homepage tab).
  const GURL base_url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(window_id, base_url,
                             LockedNavigationOptions::BLOCK_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 2);
  boca_app_browser->tab_strip_model()->ActivateTabAt(1);

  // Pause the app.
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  system_web_app_manager()->SetPauseStateForSystemWebAppWindow(/*paused=*/true,
                                                               window_id);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->active_index(), 0);

  // Enter tablet mode and verify immersive mode remains disabled even when we
  // attempt a toolbar reveal.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  auto* const immersive_mode_controller =
      boca_app_browser->GetImmersiveModeController();
  const std::unique_ptr<ImmersiveRevealedLock> reveal_lock =
      immersive_mode_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO);
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());

  // Exit tablet mode and verify immersive mode remains disabled.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerBrowserTest,
                       MultiplePauseUnpauseRequests) {
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

  // Spawn a tab for testing purposes (outside the homepage tab).
  const GURL base_url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(window_id, base_url,
                             LockedNavigationOptions::BLOCK_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 2);
  boca_app_browser->tab_strip_model()->ActivateTabAt(1);

  // Pause the app once.
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  system_web_app_manager()->SetPauseStateForSystemWebAppWindow(/*paused=*/true,
                                                               window_id);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->active_index(), 0);
  auto* const immersive_mode_controller =
      boca_app_browser->GetImmersiveModeController();
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());

  // Pause the app again and verify immersive mode remains disabled.
  system_web_app_manager()->SetPauseStateForSystemWebAppWindow(/*paused=*/true,
                                                               window_id);
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());

  // Unpause the app and verify immersive mode is enabled.
  system_web_app_manager()->SetPauseStateForSystemWebAppWindow(/*paused=*/false,
                                                               window_id);
  EXPECT_TRUE(immersive_mode_controller->IsEnabled());

  // Unpin as well as unpause the app and verify immersive mode is disabled.
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/false,
                                                             window_id);
  system_web_app_manager()->SetPauseStateForSystemWebAppWindow(/*paused=*/false,
                                                               window_id);
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
}

class OnTaskLockedSessionWindowTrackerDownloadURLBrowserTest
    : public OnTaskLockedSessionWindowTrackerBrowserTestBase {
 protected:
  void SetUpOnMainThread() override {
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
    OnTaskLockedSessionWindowTrackerBrowserTestBase::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionWindowTrackerDownloadURLBrowserTest,
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
  const GURL base_url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(window_id, base_url,
                             LockedNavigationOptions::OPEN_NAVIGATION);
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

}  // namespace
}  // namespace ash::boca
