// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/boca/on_task/notification_constants.h"
#include "chromeos/ash/components/boca/on_task/on_task_notifications_manager.h"
#include "chromeos/ash/components/boca/on_task/util/mock_clock.h"
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

// Fake delegate implementation for the `OnTaskNotificationsManager` to minimize
// dependency on Ash UI.
class FakeOnTaskNotificationsManagerDelegate
    : public boca::OnTaskNotificationsManager::Delegate {
 public:
  FakeOnTaskNotificationsManagerDelegate() = default;
  ~FakeOnTaskNotificationsManagerDelegate() override = default;

  // OnTaskNotificationsManager::Delegate:
  void ShowNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    notifications_shown_.insert(notification->id());
  }
  void ClearNotification(const std::string& notification_id) override {
    notifications_shown_.erase(notification_id);
  }

  bool WasNotificationShown(const std::string& id) {
    return notifications_shown_.contains(id);
  }

 private:
  std::set<std::string> notifications_shown_;
};

class OnTaskSessionManagerBrowserTestBase : public InProcessBrowserTest {
 protected:
  OnTaskSessionManagerBrowserTestBase() {
    // Initialize the MockClock.
    boca::MockClock::Get();
  }

  void SetUpOnMainThread() override {
    ash::SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();
    host_resolver()->AddRule("*", "127.0.0.1");
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_server()->AddDefaultHandlers(
        InProcessBrowserTest::GetChromeTestDataDir());

    // Override notification manager implementation to minimize dependency on
    // Ash UI.
    auto fake_notifications_delegate =
        std::make_unique<FakeOnTaskNotificationsManagerDelegate>();
    fake_notifications_delegate_ptr_ = fake_notifications_delegate.get();
    GetOnTaskSessionManager()->SetNotificationManagerForTesting(
        boca::OnTaskNotificationsManager::CreateForTest(
            std::move(fake_notifications_delegate)));

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    fake_notifications_delegate_ptr_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
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

  void VerifyNotificationShown(std::string notification_id,
                               bool notification_shown) {
    boca::MockClock::Get().Advance(boca::kOnTaskNotificationCountdownInterval);
    content::RunAllTasksUntilIdle();
    EXPECT_EQ(
        fake_notifications_delegate_ptr_->WasNotificationShown(notification_id),
        notification_shown);
  }

  void WaitForLockedModeCountdown() {
    // Simulate the full countdown duration to ensure generating the
    // notification.
    boca::MockClock::Get().Advance(
        ash::features::kBocaLockedModeCountdownDurationInSeconds.Get());
    content::RunAllTasksUntilIdle();

    // Simulate the full countdown duration to trigger completion.
    boca::MockClock::Get().Advance(
        ash::features::kBocaLockedModeCountdownDurationInSeconds.Get());
    content::RunAllTasksUntilIdle();
  }

  Profile* profile() { return browser()->profile(); }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  raw_ptr<FakeOnTaskNotificationsManagerDelegate>
      fake_notifications_delegate_ptr_;
};

class OnTaskSessionManagerBrowserTest
    : public OnTaskSessionManagerBrowserTestBase {
 protected:
  OnTaskSessionManagerBrowserTest() {
    // Enable Boca and consumer experience for testing purposes. This is used
    // to set up the Boca SWA for OnTask.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kBoca, features::kBocaConsumer,
                              features::kBocaLockedModeCustomCountdownDuration,
                              features::kOnDeviceSpeechRecognition,
                              features::kBocaKeepSWAOpenOnSessionEnded},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    OnTaskSessionManagerBrowserTest,
    ShouldEnforceDomainNavRestrictionOnHomepageOnSessionStart) {
  const GURL boca_url(kChromeBocaAppUntrustedIndexURL);
  content::TestNavigationObserver navigation_observer(boca_url);
  navigation_observer.StartWatchingNewWebContents();

  // Start OnTask session to spin up the SWA and the homepage.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  navigation_observer.Wait();
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 1);
  ASSERT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            boca_url);

  // Trigger an explicit URL blocklist refresh to ensure the nav restriction has
  // been applied on the homepage. This is not an issue in the real world
  // because tab interactions trigger this refresh.
  LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
      ->RefreshUrlBlocklist();
  content::RunAllTasksUntilIdle();

  // Attempt to navigate away on the home tab and verify it does not go through.
  const GURL test_url(kTestUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, test_url));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            test_url);
}

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
  WaitForLockedModeCountdown();
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
  WaitForLockedModeCountdown();
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
                       ShouldPinBocaSWAWhenPauseAndUnpauseOnBundleUpdated) {
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
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            GURL(kTestUrl1));

  // Lock the boca app.
  bundle.set_locked(true);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  WaitForLockedModeCountdown();
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));

  // Pause the boca app.
  bundle.set_lock_to_app_home(true);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  // Wait until immersive mode is disabled in pause mode.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !ImmersiveModeController::From(boca_app_browser)->IsEnabled();
  }));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            GURL(kChromeBocaAppUntrustedIndexURL));

  // Unpause the boca app.
  bundle.set_lock_to_app_home(false);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_TRUE(ImmersiveModeController::From(boca_app_browser)->IsEnabled());

  // Unlock the Boca app to unblock test teardown that involves browser window
  // close.
  bundle.set_locked(false);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  EXPECT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldSkipCountdownWhenPauseInUnlockedMode) {
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
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            GURL(kTestUrl1));

  // Lock and pause the boca app.
  bundle.set_locked(true);
  bundle.set_lock_to_app_home(true);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  // Wait until immersive mode is disabled in pause mode.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !ImmersiveModeController::From(boca_app_browser)->IsEnabled();
  }));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            GURL(kChromeBocaAppUntrustedIndexURL));

  // Unpause the boca app.
  bundle.set_lock_to_app_home(false);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_TRUE(ImmersiveModeController::From(boca_app_browser)->IsEnabled());

  // Unlock the Boca app to unblock test teardown that involves browser window
  // close.
  bundle.set_locked(false);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  EXPECT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldSkipCountdownWhenPauseDuringLockedModeCountdown) {
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
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            GURL(kTestUrl1));

  // Lock the boca app.
  bundle.set_locked(true);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  // Pause the boca app.
  bundle.set_lock_to_app_home(true);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  // Wait until immersive mode is disabled in pause mode.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !ImmersiveModeController::From(boca_app_browser)->IsEnabled();
  }));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            GURL(kChromeBocaAppUntrustedIndexURL));

  // Unpause the boca app.
  bundle.set_lock_to_app_home(false);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_TRUE(ImmersiveModeController::From(boca_app_browser)->IsEnabled());

  // Unlock the Boca app to unblock test teardown that involves browser window
  // close.
  bundle.set_locked(false);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  EXPECT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldNotLockBocaSWAInAppReloadIfLockInProgress) {
  content::TestNavigationObserver navigation_observer((GURL(kTestUrl1)));
  navigation_observer.StartWatchingNewWebContents();

  // Start OnTask session, spawn one tab outside the homepage tab and lock the
  // boca app.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.set_locked(true);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  navigation_observer.Wait();

  // Boca should not be locked before the full countdown, and locked after the
  // full countdown.
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());
  ASSERT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  WaitForLockedModeCountdown();
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

  // Navigate the active tab to the new url. Wait for pending tasks to finish
  // before attempting another navigation to ensure nav restrictions are applied
  // on the tab.
  content::RunAllTasksUntilIdle();
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

  // Navigate the active tab to the new url. Wait for pending tasks to finish
  // before attempting another navigation to ensure nav restrictions are applied
  // on the tab.
  content::RunAllTasksUntilIdle();
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
                       ShouldKeepBocaSWAOpenOnSessionEndWithFeatureEnabled) {
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

  VerifyNotificationShown(kOnTaskSessionEndNotificationId, true);
  VerifyNotificationShown(kOnTaskBundleContentAddedNotificationId, false);
  VerifyNotificationShown(kOnTaskBundleContentRemovedNotificationId, false);
  EXPECT_EQ(FindBocaSystemWebAppBrowser(), boca_app_browser);
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       RestoreTabsSentByProviderOnAppReload) {
  content::TestNavigationObserver navigation_observer((GURL(kTestUrl2)));
  navigation_observer.StartWatchingNewWebContents();

  // Start OnTask session and spawn two tabs outside the homepage tab.
  GetOnTaskSessionManager()->OnSessionStarted(kSessionId,
                                              ::boca::UserIdentity());
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);

  // Add more content but set the navigation type to be 'One link away from this
  // page' for testing purposes.
  ::boca::ContentConfig* const content_config =
      bundle.mutable_content_configs()->Add();
  content_config->set_url(kTestUrl2);
  content_config->mutable_locked_navigation_options()->set_navigation_type(
      LockedNavigationOptions::LIMITED_NAVIGATION);
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

  // Open a new tab that is not sent by the provider. Wait until nav
  // restrictions have been applied on the tab.
  content::RunAllTasksUntilIdle();
  const GURL new_url(embedded_test_server()->GetURL("/test/new_page.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      boca_app_browser, new_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            new_url);
  ASSERT_EQ(tab_strip_model->count(), 4);

  // Attempt an app reload and verify tabs sent by the provider are restored.
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
                       ShouldMuteAndUnmuteTabsAudioWhenLockAndUnlock) {
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

  // Tabs in other browsers are unmuted.
  EXPECT_FALSE(
      browser_1->tab_strip_model()->GetActiveWebContents()->IsAudioMuted());
  EXPECT_FALSE(
      browser_2->tab_strip_model()->GetActiveWebContents()->IsAudioMuted());
}

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerBrowserTest,
                       ShouldRespectLatestPinStateOnBundleUpdated) {
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
  ASSERT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  // Pause and then unlock the boca app.
  bundle.set_locked(true);
  bundle.set_lock_to_app_home(true);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  bundle.set_locked(false);
  bundle.set_lock_to_app_home(false);
  GetOnTaskSessionManager()->OnBundleUpdated(bundle);
  EXPECT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
}

class OnTaskSessionManagerCloseSWAOnSessionEndBrowserTest
    : public OnTaskSessionManagerBrowserTestBase {
 protected:
  OnTaskSessionManagerCloseSWAOnSessionEndBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kBoca, features::kBocaConsumer,
                              features::kBocaLockedModeCustomCountdownDuration,
                              features::kOnDeviceSpeechRecognition},
        /*disabled_features=*/{features::kBocaKeepSWAOpenOnSessionEnded});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OnTaskSessionManagerCloseSWAOnSessionEndBrowserTest,
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
  VerifyNotificationShown(kOnTaskSessionEndNotificationId, true);
  VerifyNotificationShown(kOnTaskBundleContentAddedNotificationId, false);
  VerifyNotificationShown(kOnTaskBundleContentRemovedNotificationId, false);
  ASSERT_THAT(FindBocaSystemWebAppBrowser(), IsNull());
}

}  // namespace
}  // namespace ash::boca
