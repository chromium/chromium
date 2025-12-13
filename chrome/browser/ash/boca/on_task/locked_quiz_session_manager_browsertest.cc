// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/locked_quiz_session_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/boca/on_task/locked_quiz_session_manager.h"
#include "chrome/browser/ash/boca/on_task/locked_quiz_session_manager_factory.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ui/wm/window_util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::boca::LockedNavigationOptions;
using ::testing::IsNull;
using ::testing::NotNull;

namespace ash::boca {
namespace {

constexpr char kQuizUrl1[] = "https://docs.google.com/forms/startquiz1";
constexpr char kQuizUrl2[] = "https://docs.google.com/forms/startquiz2";

class LockedQuizSessionManagerBrowserTestBase : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ash::SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();
    host_resolver()->AddRule("*", "127.0.0.1");
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    embedded_test_server()->AddDefaultHandlers(
        InProcessBrowserTest::GetChromeTestDataDir());
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  LockedQuizSessionManager* GetLockedQuizSessionManager() {
    return boca::LockedQuizSessionManagerFactory::GetForBrowserContext(
        profile());
  }

  Browser* FindBocaSystemWebAppBrowser() {
    return ash::FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::BOCA);
  }

  Profile* profile() { return browser()->profile(); }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
};

class LockedQuizSessionManagerBrowserTest
    : public LockedQuizSessionManagerBrowserTestBase {
 protected:
  LockedQuizSessionManagerBrowserTest() {
    // Enable Boca and consumer experience for testing purposes. This is used
    // to set up the Boca SWA for Locked Quiz.
    // TODO(crbug.com/438844429): Remove `kBoca` and `kBocaConsumer` feature
    // flags once Boca SWA is installed even when Class Tools policy is not set.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kBoca, features::kBocaConsumer},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LockedQuizSessionManagerBrowserTest,
                       ShouldOpenLockedQuizWhenNoExistingWindow) {
  content::TestNavigationObserver navigation_observer_1((GURL(kQuizUrl1)));
  navigation_observer_1.StartWatchingNewWebContents();
  base::test::TestFuture<const SessionID&> future;
  GetLockedQuizSessionManager()->OpenLockedQuiz(GURL(kQuizUrl1),
                                                future.GetCallback());
  Browser* const boca_app_browser =
      BrowserWindowInterface::FromSessionID(future.Get())
          ->GetBrowserForMigrationOnly();
  navigation_observer_1.Wait();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_EQ(boca_app_browser, FindBocaSystemWebAppBrowser());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));

  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 1);
  content::WebContents* const tab = tab_strip_model->GetActiveWebContents();
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  EXPECT_EQ(tab->GetLastCommittedURL(), GURL(kQuizUrl1));
  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id],
            LockedNavigationOptions::DOMAIN_NAVIGATION);
}

IN_PROC_BROWSER_TEST_F(LockedQuizSessionManagerBrowserTest,
                       ShouldOpenLockedQuizWhenExistingWindow) {
  content::TestNavigationObserver navigation_observer_1((GURL(kQuizUrl1)));
  navigation_observer_1.StartWatchingNewWebContents();
  content::TestNavigationObserver navigation_observer_2((GURL(kQuizUrl2)));
  navigation_observer_2.StartWatchingNewWebContents();

  base::test::TestFuture<const SessionID&> future_1;
  GetLockedQuizSessionManager()->OpenLockedQuiz(GURL(kQuizUrl1),
                                                future_1.GetCallback());
  Browser* const boca_app_browser_1 =
      BrowserWindowInterface::FromSessionID(future_1.Get())
          ->GetBrowserForMigrationOnly();
  navigation_observer_1.Wait();
  ASSERT_THAT(boca_app_browser_1, NotNull());
  ASSERT_EQ(boca_app_browser_1, FindBocaSystemWebAppBrowser());
  ASSERT_TRUE(boca_app_browser_1->IsLockedForOnTask());

  base::test::TestFuture<const SessionID&> future_2;
  GetLockedQuizSessionManager()->OpenLockedQuiz(GURL(kQuizUrl2),
                                                future_2.GetCallback());
  Browser* const boca_app_browser_2 =
      BrowserWindowInterface::FromSessionID(future_2.Get())
          ->GetBrowserForMigrationOnly();
  navigation_observer_2.Wait();
  ASSERT_THAT(boca_app_browser_2, NotNull());
  ASSERT_EQ(boca_app_browser_2, FindBocaSystemWebAppBrowser());
  ASSERT_TRUE(boca_app_browser_2->IsLockedForOnTask());
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser_2));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser_2->window()->GetNativeWindow()));

  auto* const tab_strip_model = boca_app_browser_2->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 1);
  content::WebContents* const tab = tab_strip_model->GetActiveWebContents();
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  EXPECT_EQ(tab->GetLastCommittedURL(), GURL(kQuizUrl2));
  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  EXPECT_EQ(on_task_blocklist->parent_tab_to_nav_filters()[tab_id],
            LockedNavigationOptions::DOMAIN_NAVIGATION);
}

}  // namespace
}  // namespace ash::boca
