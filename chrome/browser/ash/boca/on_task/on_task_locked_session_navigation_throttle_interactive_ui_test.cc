// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <set>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/wm/window_state.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_controller.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_navigation_throttle.h"
#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/boca/on_task/notification_constants.h"
#include "chromeos/ash/components/boca/on_task/on_task_notifications_manager.h"
#include "chromeos/ash/components/boca/on_task/util/mock_clock.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "chromeos/ash/components/system_web_apps/system_web_app_type.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/common/extension_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using ::testing::NotNull;

namespace ash {
namespace {

constexpr char kTabUrl1Host[] = "www.example.com";
constexpr char kTabUrl1SubDomainHost[] = "example.child.com";
constexpr char kTabUrl1FrontSubDomainHost[] = "sub.example.com";
constexpr char kTabUrl2Host[] = "www.company.org";
constexpr char kTabUrlRedirectHost[] = "redirect-url.com";
constexpr char kTabGoogleHost[] = "www.google.com";
constexpr char kTabGoogleDocsHost[] = "docs.google.com";

// Fake delegate implementation for the `OnTaskNotificationsManager` to minimize
// dependency on Ash UI.
class FakeOnTaskNotificationsManagerDelegate
    : public boca::OnTaskNotificationsManager::Delegate {
 public:
  FakeOnTaskNotificationsManagerDelegate() = default;
  ~FakeOnTaskNotificationsManagerDelegate() override = default;

  void ShowToast(ash::ToastData toast_data) override {
    notifications_shown_.insert(toast_data.id);
  }

  bool WasNotificationShown(const std::string& id) {
    return notifications_shown_.contains(id);
  }

 private:
  std::set<std::string> notifications_shown_;
};

// Fake implementation of IdentityCredentialSource to simplify testing FedCM
// oauth flows.
class FakeIdentityCredentialSource
    : public content::webid::IdentityCredentialSource {
 public:
  FakeIdentityCredentialSource() = default;
  ~FakeIdentityCredentialSource() override = default;

  void GetIdentityCredentialSuggestions(
      const std::vector<GURL>& embedder_requested_idps,
      GetIdentityCredentialSuggestionsCallback callback) override {}

  bool HasPendingRequest() override { return true; }

  bool SelectAccount(const url::Origin& idp_origin,
                     const std::string& account_id) override {
    return true;
  }

  void SetEmbedderLoginRequest(
      const url::Origin& idp_origin,
      const std::string& account_id,
      base::OnceCallback<void(content::webid::FederatedLoginResult)> callback)
      override {}
};

class OnTaskLockedSessionNavigationThrottleInteractiveUITestBase
    : public InProcessBrowserTest {
 protected:
  OnTaskLockedSessionNavigationThrottleInteractiveUITestBase() {
    // Initialize the MockClock.
    boca::MockClock::Get();

    // Enable Boca and consumer experience for testing purposes. This is used
    // to set up the Boca SWA for OnTask.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kBoca, features::kBocaConsumer,
                              features::kOnDeviceSpeechRecognition},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();
    system_web_app_manager_ =
        std::make_unique<boca::OnTaskSystemWebAppManagerImpl>(profile());

    // Set up notifications manager with the fake delegate for testing purposes.
    auto fake_notifications_delegate =
        std::make_unique<FakeOnTaskNotificationsManagerDelegate>();
    fake_notifications_delegate_ptr_ = fake_notifications_delegate.get();
    LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
        ->SetNotificationManagerForTesting(
            boca::OnTaskNotificationsManager::CreateForTest(
                std::move(fake_notifications_delegate)));
  }

  void TearDownOnMainThread() override {
    system_web_app_manager_.reset();
    fake_notifications_delegate_ptr_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Creates a new background tab with the specified url and navigation
  // restrictions, and waits until the specified url has been loaded.
  // Returns the newly created tab id.
  SessionID CreateBackgroundTabAndWait(
      SessionID window_id,
      const GURL& url,
      ::boca::LockedNavigationOptions::NavigationType restriction_level) {
    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.StartWatchingNewWebContents();
    const SessionID tab_id =
        system_web_app_manager()->CreateBackgroundTabWithUrl(window_id, url,
                                                             restriction_level);
    navigation_observer.Wait();
    return tab_id;
  }

  void SpawnChildTabWithURL(
      const BrowserWindowInterface* browser,
      const GURL& url,
      std::optional<std::string> window_features = std::nullopt,
      bool ignore_uncommitted_navigations = true) {
    content::WebContents* const active_web_contents =
        browser->GetTabStripModel()->GetActiveWebContents();
    content::TestNavigationObserver navigation_observer(
        active_web_contents, 1, content::MessageLoopRunner::QuitMode::IMMEDIATE,
        ignore_uncommitted_navigations);
    navigation_observer.StartWatchingNewWebContents();
    std::string js_code;
    if (window_features.has_value()) {
      js_code = content::JsReplace("window.open($1, '_blank', $2);", url,
                                   window_features.value());
    } else {
      js_code = content::JsReplace("window.open($1, '_blank');", url);
    }
    ASSERT_TRUE(content::ExecJs(active_web_contents, js_code));
    navigation_observer.WaitForNavigationFinished();
  }

  BrowserWindowInterface* FindBocaSystemWebAppBrowser() {
    ash::BrowserDelegate* delegate = FindSystemWebAppBrowser(
        profile(), SystemWebAppType::BOCA, ash::BrowserType::kApp);
    return delegate ? &delegate->GetBrowser() : nullptr;
  }

  void VerifyUrlBlockedToastShown(bool toast_was_shown) {
    boca::MockClock::Get().Advance(boca::kOnTaskNotificationCountdownInterval);
    content::RunAllTasksUntilIdle();
    EXPECT_EQ(fake_notifications_delegate_ptr_->WasNotificationShown(
                  boca::kOnTaskUrlBlockedToastId),
              toast_was_shown);
  }

  void WaitForUrlBlocklistUpdate() {
    // We rely on web content updates reported by the tab strip model to update
    // the OnTask URL blocklist. We advance the timer and wait for these updates
    // here.
    boca::MockClock::Get().Advance(base::Seconds(1));
    content::RunAllTasksUntilIdle();
  }

  Profile* profile() { return browser()->GetProfile(); }

  boca::OnTaskSystemWebAppManagerImpl* system_web_app_manager() const {
    return system_web_app_manager_.get();
  }

 private:
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<boca::OnTaskSystemWebAppManagerImpl> system_web_app_manager_;
  raw_ptr<FakeOnTaskNotificationsManagerDelegate>
      fake_notifications_delegate_ptr_;
};

class OnTaskLockedSessionNavigationThrottleInteractiveUITest
    : public OnTaskLockedSessionNavigationThrottleInteractiveUITestBase {
 protected:
  void SetUpOnMainThread() override {
    OnTaskLockedSessionNavigationThrottleInteractiveUITestBase::
        SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       AllowFormSubmission) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window. This is needed to activate
  // the navigation throttle.
  const SessionID window_id = boca_app_browser->GetSessionID();
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Open and submit form. Verify the form was submitted by looking at the
  // visible URL (in case the navigation has not been committed yet).
  const GURL form_url(embedded_test_server()->GetURL("/form.html"));
  CreateBackgroundTabAndWait(window_id, form_url,
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      boca_app_browser,
      GURL("javascript:document.getElementById('form').submit()")));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetVisibleURL(), form_url);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       BlockCWSAccess) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window. This is needed to activate
  // the navigation throttle.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  const GURL tab_url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(window_id, tab_url,
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);

  // Attempt to navigate to CWS and verify it is blocked.
  tab_strip_model->ActivateTabAt(1);
  ASSERT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            tab_url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      boca_app_browser, extension_urls::GetWebstoreLaunchURL()));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            tab_url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      boca_app_browser, extension_urls::GetNewWebstoreLaunchURL()));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            tab_url);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       NoNavigationThrottleRegisteredWithoutTracker) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  const SessionID window_id = boca_app_browser->GetSessionID();
  CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl1Host, "/"),
      ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);

  // Verify nav throttle is not created even for navigations that are outside
  // the specified restriction.
  tab_strip_model->ActivateTabAt(1);
  content::RunAllTasksUntilIdle();
  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, different_domain_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       BlockUrlsForBlockNavRestriction) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  CreateBackgroundTabAndWait(window_id,
                             embedded_test_server()->GetURL(kTabUrl1Host, "/"),
                             ::boca::LockedNavigationOptions::BLOCK_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Navigate to other URLs and verify they are blocked.
  const GURL url_1_front_subdomain =
      embedded_test_server()->GetURL(kTabUrl1FrontSubDomainHost, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, url_1_front_subdomain));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_front_subdomain);

  const GURL url_1_with_path =
      embedded_test_server()->GetURL(kTabUrl1Host, "/some-path");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, url_1_with_path));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_with_path);

  const GURL url_1_subdomain =
      embedded_test_server()->GetURL(kTabUrl1SubDomainHost, "/some-path");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, url_1_subdomain));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_subdomain);

  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, different_domain_url));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       AllowNavigationsToBocaHomepage) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  CreateBackgroundTabAndWait(window_id,
                             embedded_test_server()->GetURL(kTabUrl1Host, "/"),
                             ::boca::LockedNavigationOptions::BLOCK_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Navigate to Boca homepage and verify it goes through.
  const GURL boca_app_url(boca::kChromeBocaAppUntrustedURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, boca_app_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            boca_app_url);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/false);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       AllowUrlsForOpenNavRestriction) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  CreateBackgroundTabAndWait(window_id,
                             embedded_test_server()->GetURL(kTabUrl1Host, "/"),
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Navigate to other URLs and verify they are not blocked.
  const GURL url_1_front_subdomain =
      embedded_test_server()->GetURL(kTabUrl1FrontSubDomainHost, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, url_1_front_subdomain));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_front_subdomain);

  const GURL url_1_with_path =
      embedded_test_server()->GetURL(kTabUrl1Host, "/some-path");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, url_1_with_path));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_with_path);

  const GURL url_1_subdomain =
      embedded_test_server()->GetURL(kTabUrl1SubDomainHost, "/some-path");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, url_1_subdomain));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_subdomain);

  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, different_domain_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/false);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       AllowAndBlockUrlsForSameDomainNavRestriction) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl1Host, "/"),
      ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Navigate to URLs that should be allowed to go through.
  const GURL url_1_with_path =
      embedded_test_server()->GetURL(kTabUrl1Host, "/some-path");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, url_1_with_path));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_with_path);
  WaitForUrlBlocklistUpdate();

  const GURL url_1_front_subdomain =
      embedded_test_server()->GetURL(kTabUrl1FrontSubDomainHost, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, url_1_front_subdomain));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_front_subdomain);
  WaitForUrlBlocklistUpdate();
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/false);

  // Navigate to URLs that should be blocked.
  const GURL url_1_subdomain =
      embedded_test_server()->GetURL(kTabUrl1SubDomainHost, "/some-path");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, url_1_subdomain));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_subdomain);
  WaitForUrlBlocklistUpdate();

  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, different_domain_url));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       AllowAndBlockUrlsForOneLevelDeepNavRestriction) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl1Host, "/"),
      ::boca::LockedNavigationOptions::LIMITED_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Navigate one level deep and verify it goes through.
  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, different_domain_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
  WaitForUrlBlocklistUpdate();
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/false);

  // Attempt to navigate one more level and verify it does not go through.
  const GURL different_domain_url_with_path =
      embedded_test_server()->GetURL(kTabUrl2Host, "/some-page");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser,
                                           different_domain_url_with_path));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url_with_path);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       AllowAndBlockUrlsForOneLevelDeepNavRestrictionOnNewTab) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  const GURL tab_url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(
      window_id, tab_url, ::boca::LockedNavigationOptions::LIMITED_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Spawn a child tab to simulate opening a new link.
  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
  SpawnChildTabWithURL(boca_app_browser, different_domain_url);
  ASSERT_EQ(tab_strip_model->count(), 3);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
  WaitForUrlBlocklistUpdate();

  // Any further navigation attempts to other URLs should fail on this child tab
  // because we have already met the 1LD requirement.
  const GURL different_domain_url_with_path =
      embedded_test_server()->GetURL(kTabUrl2Host, "/some-page");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser,
                                           different_domain_url_with_path));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url_with_path);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);
  WaitForUrlBlocklistUpdate();

  // Switch to the parent tab and verify we can still navigate one level deep.
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, different_domain_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       BackgroundTabNavigationEnforcesCorrectNavRestriction) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn foreground tab (Tab 1) with OPEN_NAVIGATION.
  const GURL tab_url_1 = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(window_id, tab_url_1,
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);

  // Spawn background tab (Tab 2) with BLOCK_NAVIGATION.
  const GURL tab_url_2 = embedded_test_server()->GetURL(kTabUrl2Host, "/");
  CreateBackgroundTabAndWait(window_id, tab_url_2,
                             ::boca::LockedNavigationOptions::BLOCK_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 3);

  // Activate Tab 1 (so Tab 2 is in the background).
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Attempt to navigate Tab 2 (background) to a different URL.
  content::WebContents* const background_contents =
      tab_strip_model->GetWebContentsAt(2);
  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl1Host, "/some-page");
  EXPECT_FALSE(
      content::NavigateToURL(background_contents, different_domain_url));

  // Verify the navigation was blocked on the background tab.
  EXPECT_EQ(background_contents->GetLastCommittedURL(), tab_url_2);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       BackgroundTabNavigationEnforcesUnknownNavRestriction) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn foreground tab (Tab 1) with OPEN_NAVIGATION.
  const GURL tab_url_1 = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(window_id, tab_url_1,
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);

  // Spawn background tab (Tab 2) with NAVIGATION_TYPE_UNKNOWN.
  const GURL tab_url_2 = embedded_test_server()->GetURL(kTabUrl2Host, "/");
  CreateBackgroundTabAndWait(
      window_id, tab_url_2,
      ::boca::LockedNavigationOptions::NAVIGATION_TYPE_UNKNOWN);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 3);

  // Activate Tab 1 (so Tab 2 is in the background).
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Attempt to navigate Tab 2 (background) to a different URL.
  content::WebContents* const background_contents =
      tab_strip_model->GetWebContentsAt(2);
  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl1Host, "/some-page");
  EXPECT_FALSE(
      content::NavigateToURL(background_contents, different_domain_url));

  // Verify the navigation was blocked on the background tab.
  EXPECT_EQ(background_contents->GetLastCommittedURL(), tab_url_2);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       BlockNewChildTabWhenParentOneLevelDeepQuotaIsConsumed) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn parent tab with LIMITED_NAVIGATION.
  const GURL parent_url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(
      window_id, parent_url,
      ::boca::LockedNavigationOptions::LIMITED_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Navigate the parent tab to a different URL to consume its 1LD quota.
  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, different_domain_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
  WaitForUrlBlocklistUpdate();

  // Now try to spawn a child tab from the parent tab. The new tab navigation
  // should be blocked since the parent's budget is consumed.
  const GURL different_domain_url_with_path =
      embedded_test_server()->GetURL(kTabUrl2Host, "/some-page");
  SpawnChildTabWithURL(boca_app_browser, different_domain_url_with_path,
                       /*window_features=*/std::nullopt,
                       /*ignore_uncommitted_navigations=*/false);
  content::RunAllTasksUntilIdle();

  // Since the navigation was blocked, the new tab should have been
  // closed/removed by the window tracker.
  EXPECT_EQ(tab_strip_model->count(), 2);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);
}

IN_PROC_BROWSER_TEST_F(
    OnTaskLockedSessionNavigationThrottleInteractiveUITest,
    BlockNewNoopenerChildTabWhenParentOneLevelDeepQuotaIsConsumed) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn parent tab with LIMITED_NAVIGATION.
  const GURL parent_url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(
      window_id, parent_url,
      ::boca::LockedNavigationOptions::LIMITED_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Navigate the parent tab to a different URL to consume its 1LD quota.
  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, different_domain_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
  WaitForUrlBlocklistUpdate();

  // Now try to spawn a child tab from the parent tab using noopener. The new
  // tab navigation should be blocked since the parent's budget is consumed.
  const GURL different_domain_url_with_path =
      embedded_test_server()->GetURL(kTabUrl2Host, "/some-page");
  SpawnChildTabWithURL(boca_app_browser, different_domain_url_with_path,
                       /*window_features=*/"noopener",
                       /*ignore_uncommitted_navigations=*/false);
  content::RunAllTasksUntilIdle();

  // Since the navigation was blocked, the new tab should have been
  // closed/removed by the window tracker.
  EXPECT_EQ(tab_strip_model->count(), 2);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);
}

IN_PROC_BROWSER_TEST_F(
    OnTaskLockedSessionNavigationThrottleInteractiveUITest,
    AllowAndBlockUrlsForSameDomainAndOneLevelDeepNavRestriction) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  const GURL url_1 = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(
      window_id, url_1,
      ::boca::LockedNavigationOptions::
          SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  auto* const on_task_blocklist =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile())
          ->on_task_blocklist();
  ASSERT_TRUE(on_task_blocklist->CanPerformOneLevelNavigation(
      tab_strip_model->GetActiveWebContents()));

  // Navigate to a subdomain and verify it goes through.
  const GURL url_1_front_subdomain =
      embedded_test_server()->GetURL(kTabUrl1FrontSubDomainHost, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, url_1_front_subdomain));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_front_subdomain);
  ASSERT_TRUE(on_task_blocklist->CanPerformOneLevelNavigation(
      tab_strip_model->GetActiveWebContents()));
  WaitForUrlBlocklistUpdate();

  // Any attempt to navigate to a different domain should also go through (one
  // level deep).
  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, different_domain_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
  ASSERT_FALSE(on_task_blocklist->CanPerformOneLevelNavigation(
      tab_strip_model->GetActiveWebContents()));
  WaitForUrlBlocklistUpdate();

  // Subsequent navigation attempts to a different domain should be blocked.
  const GURL different_domain_url_with_path =
      embedded_test_server()->GetURL(kTabUrl2Host, "/some-path");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser,
                                           different_domain_url_with_path));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url_with_path);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);
}

IN_PROC_BROWSER_TEST_F(
    OnTaskLockedSessionNavigationThrottleInteractiveUITest,
    AllowAndBlockUrlsForSameDomainAndOneLevelDeepNavRestrictionOnNewPage) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  const GURL tab_url = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(
      window_id, tab_url,
      ::boca::LockedNavigationOptions::
          SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Spawn a child tab to simulate opening a new link but with the same URL as
  // the parent tab.
  SpawnChildTabWithURL(boca_app_browser, tab_url);
  ASSERT_EQ(tab_strip_model->count(), 3);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            tab_url);
  WaitForUrlBlocklistUpdate();

  // Attempt to navigate to a subdomain and verify it goes through.
  const GURL url_1_front_subdomain =
      embedded_test_server()->GetURL(kTabUrl1Host, "/some-page");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, url_1_front_subdomain));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_front_subdomain);
  WaitForUrlBlocklistUpdate();

  // Attempt to navigate one level deep to a different domain and verify it goes
  // through.
  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, different_domain_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
  WaitForUrlBlocklistUpdate();

  // Any further navigation attempts to other URLs should fail on this tab.
  const GURL different_domain_url_with_path =
      embedded_test_server()->GetURL(kTabUrl2Host, "/some-page");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser,
                                           different_domain_url_with_path));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url_with_path);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);

  // Switch back to the parent tab and navigate 1LD by spawning a new child tab.
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  SpawnChildTabWithURL(boca_app_browser, different_domain_url);
  ASSERT_EQ(tab_strip_model->count(), 4);
  ASSERT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
  WaitForUrlBlocklistUpdate();

  // Any further navigation on this tab should be blocked.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser,
                                           different_domain_url_with_path));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url_with_path);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       ClosePopupIfNotOauth) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  // Spawn popup and verify it gets closed on nav completion.
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());
  ASSERT_TRUE(window_tracker->CanOpenNewPopup());
  const size_t original_browser_count =
      GlobalBrowserCollection::GetInstance()->GetSize();
  BrowserWindowInterface* const popup_browser = CreateBrowserWindow(
      BrowserWindowCreateParams(BrowserWindowInterface::TYPE_APP_POPUP,
                                profile(), /*from_user_gesture=*/true));
  content::RunAllTasksUntilIdle();
  ASSERT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(),
            original_browser_count + 1);
  EXPECT_FALSE(window_tracker->CanOpenNewPopup());

  ui_test_utils::BrowserDestroyedObserver popup_closed_observer(popup_browser);
  NavigateParams navigate_params(
      popup_browser, embedded_test_server()->GetURL(kTabUrl1Host, "/"),
      ui::PAGE_TRANSITION_LINK);
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  ui_test_utils::NavigateToURL(&navigate_params);
  popup_closed_observer.Wait();
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(),
            original_browser_count);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       RejectNonHttpOauthLoginSpoofing) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());

  // Navigate to a URL that has client_id but uses a non-HTTP scheme
  // (e.g. data:). It should NOT be recognized as OAuth start and should not
  // trigger oauth_in_progress.
  content::TestNavigationObserver nav_observer(
      boca_app_browser->GetTabStripModel()->GetActiveWebContents(), 1);

  ASSERT_TRUE(content::ExecJs(
      boca_app_browser->GetTabStripModel()->GetActiveWebContents(),
      "window.location.href = 'data:text/html,hello?client_id=123';"));

  nav_observer.Wait();

  EXPECT_FALSE(window_tracker->oauth_in_progress());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       AllowOauthPopups) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  // Spawn tab for testing purposes.
  const GURL url_1 = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(
      window_id, url_1, ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Spawn popup and simulate oauth request.
  //
  // In order to successfully simulate an oauth flow, we use an interceptor to
  // allow referencing arbitrary paths on the origin used to process the oauth
  // request without worrying that the corresponding test files exist.
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.GetHost() == kTabUrlRedirectHost) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/title2.html", params->client.get());
          return true;
        }
        // Not handled by us.
        return false;
      }));

  const size_t original_browser_count =
      GlobalBrowserCollection::GetInstance()->GetSize();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());
  ASSERT_TRUE(window_tracker->CanOpenNewPopup());
  NavigateParams navigate_params(
      boca_app_browser,
      embedded_test_server()->GetURL(
          kTabUrlRedirectHost,
          "/authenticate?client_id=123&response_type=code"),
      ui::PAGE_TRANSITION_LINK);
  navigate_params.disposition = WindowOpenDisposition::NEW_POPUP;
  navigate_params.window_action = NavigateParams::WindowAction::kShowWindow;
  ui_test_utils::NavigateToURL(&navigate_params);
  BrowserWindowInterface* const popup_browser = navigate_params.browser;

  ui_test_utils::BrowserActivationWaiter popup_activation_waiter(popup_browser);
  popup_activation_waiter.WaitForActivation();
  ASSERT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(),
            original_browser_count + 1);
  EXPECT_FALSE(popup_browser->IsDeleteScheduled());
  EXPECT_FALSE(window_tracker->CanOpenNewPopup());
  ASSERT_TRUE(window_tracker->oauth_in_progress());

  // The oauth popup in reality should close once the login flow is complete.
  // This is normally done through a redirect with an auto close window query,
  // but we simulate this in the test.
  ui_test_utils::BrowserDestroyedObserver popup_closed_observer(popup_browser);
  window_tracker->set_oauth_in_progress(false, nullptr);
  ui_test_utils::NavigateToURLWithDisposition(
      popup_browser,
      embedded_test_server()->GetURL(kTabUrlRedirectHost,
                                     "/redirect?code=secret"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);
  popup_closed_observer.Wait();
  EXPECT_TRUE(window_tracker->CanOpenNewPopup());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       AllowFedCmOauthPopups) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  // Spawn tab for testing purposes.
  const GURL url_1 = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(
      window_id, url_1, ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Set up fake IdentityCredentialSource to stub FedCM requests.
  FakeIdentityCredentialSource fake_credential_source;
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());
  window_tracker->SetIdentityCredentialSourceForTesting(
      &fake_credential_source);

  // Spawn oauth popup. FedCM flow should be automatically detected when the
  // popup window is created and loaded.
  const size_t original_browser_count =
      GlobalBrowserCollection::GetInstance()->GetSize();
  ASSERT_TRUE(window_tracker->CanOpenNewPopup());
  NavigateParams navigate_params(
      boca_app_browser,
      embedded_test_server()->GetURL(kTabUrl1Host, "/title2.html"),
      ui::PAGE_TRANSITION_LINK);
  navigate_params.disposition = WindowOpenDisposition::NEW_POPUP;
  navigate_params.window_action = NavigateParams::WindowAction::kShowWindow;
  ui_test_utils::NavigateToURL(&navigate_params);
  BrowserWindowInterface* const popup_browser = navigate_params.browser;
  ui_test_utils::BrowserActivationWaiter popup_activation_waiter(popup_browser);
  popup_activation_waiter.WaitForActivation();
  ASSERT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(),
            original_browser_count + 1);
  EXPECT_FALSE(window_tracker->CanOpenNewPopup());
  ASSERT_TRUE(window_tracker->oauth_in_progress());
  content::RunAllTasksUntilIdle();

  // Simulate FedCM oauth completion and verify that the popup window is closed.
  ui_test_utils::BrowserDestroyedObserver popup_closed_observer(popup_browser);
  window_tracker->TriggerFedCmFederatedLoginCompletionForTesting(true);
  popup_closed_observer.Wait();
  EXPECT_TRUE(window_tracker->CanOpenNewPopup());
  EXPECT_FALSE(window_tracker->oauth_in_progress());

  window_tracker->SetIdentityCredentialSourceForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       AllowFedCmOAuthInSameWindow) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Set up fake IdentityCredentialSource to stub FedCM requests.
  FakeIdentityCredentialSource fake_credential_source;
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());
  window_tracker->SetIdentityCredentialSourceForTesting(
      &fake_credential_source);

  // Simulate FedCM oauth request in the same tab.
  const size_t original_browser_count =
      GlobalBrowserCollection::GetInstance()->GetSize();
  ASSERT_TRUE(window_tracker->CanOpenNewPopup());
  NavigateParams navigate_params(
      boca_app_browser,
      embedded_test_server()->GetURL(kTabUrl1Host, "/title2.html"),
      ui::PAGE_TRANSITION_LINK);
  navigate_params.disposition = WindowOpenDisposition::CURRENT_TAB;
  ui_test_utils::NavigateToURL(&navigate_params);

  // While we do not explicitly track this oauth request, we can verify that the
  // window is not scheduled for deletion.
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(),
            original_browser_count);
  EXPECT_TRUE(window_tracker->CanOpenNewPopup());
  EXPECT_FALSE(window_tracker->oauth_in_progress());

  // Simulate FedCM oauth completion. The window should still remain intact.
  window_tracker->TriggerFedCmFederatedLoginCompletionForTesting(true);
  EXPECT_TRUE(window_tracker->CanOpenNewPopup());
  EXPECT_FALSE(window_tracker->oauth_in_progress());

  window_tracker->SetIdentityCredentialSourceForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       CloseUnauthorizedPopupsDuringOAuth) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  // Spawn tab for testing purposes.
  const GURL url_1 = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(
      window_id, url_1, ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Spawn popup and simulate oauth request.
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.GetHost() == kTabUrlRedirectHost) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/title2.html", params->client.get());
          return true;
        }
        return false;
      }));

  const size_t original_browser_count =
      GlobalBrowserCollection::GetInstance()->GetSize();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());
  ASSERT_TRUE(window_tracker->CanOpenNewPopup());
  NavigateParams navigate_params(
      boca_app_browser,
      embedded_test_server()->GetURL(
          kTabUrlRedirectHost,
          "/authenticate?client_id=123&response_type=code"),
      ui::PAGE_TRANSITION_LINK);
  navigate_params.disposition = WindowOpenDisposition::NEW_POPUP;
  navigate_params.window_action = NavigateParams::WindowAction::kShowWindow;
  ui_test_utils::NavigateToURL(&navigate_params);
  BrowserWindowInterface* const popup_browser = navigate_params.browser;

  ui_test_utils::BrowserActivationWaiter popup_activation_waiter(popup_browser);
  popup_activation_waiter.WaitForActivation();
  ASSERT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(),
            original_browser_count + 1);
  EXPECT_FALSE(window_tracker->CanOpenNewPopup());
  ASSERT_TRUE(window_tracker->oauth_in_progress());

  // Spawn an unauthorized popup. It should be eventually closed by the window
  // tracker.
  NavigateParams unauthorized_nav_params(
      boca_app_browser, embedded_test_server()->GetURL(kTabUrl2Host, "/"),
      ui::PAGE_TRANSITION_LINK);
  unauthorized_nav_params.disposition = WindowOpenDisposition::NEW_POPUP;
  unauthorized_nav_params.window_action =
      NavigateParams::WindowAction::kShowWindow;
  ui_test_utils::NavigateToURL(&unauthorized_nav_params);
  BrowserWindowInterface* const unauthorized_popup =
      unauthorized_nav_params.browser;
  ui_test_utils::BrowserDestroyedObserver unauthorized_popup_closed_observer(
      unauthorized_popup);
  unauthorized_popup_closed_observer.Wait();

  // The authorized oauth popup and SWA window should still be open.
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(),
            original_browser_count + 1);

  window_tracker->set_oauth_in_progress(false, nullptr);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       CloseUnauthorizedRegularWindowsDuringOAuth) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());

  const size_t original_browser_count =
      GlobalBrowserCollection::GetInstance()->GetSize();
  ASSERT_TRUE(window_tracker->CanOpenNewPopup());
  ASSERT_FALSE(window_tracker->oauth_in_progress());

  // Spawn an authorized oauth popup.
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.GetHost() == kTabUrlRedirectHost) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/title2.html", params->client.get());
          return true;
        }
        return false;
      }));

  NavigateParams navigate_params(
      boca_app_browser,
      embedded_test_server()->GetURL(
          kTabUrlRedirectHost,
          "/authenticate?client_id=123&response_type=code"),
      ui::PAGE_TRANSITION_LINK);
  navigate_params.disposition = WindowOpenDisposition::NEW_POPUP;
  navigate_params.window_action = NavigateParams::WindowAction::kShowWindow;
  ui_test_utils::NavigateToURL(&navigate_params);
  BrowserWindowInterface* const popup_browser = navigate_params.browser;

  ui_test_utils::BrowserActivationWaiter popup_activation_waiter(popup_browser);
  popup_activation_waiter.WaitForActivation();
  ASSERT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(),
            original_browser_count + 1);
  EXPECT_FALSE(window_tracker->CanOpenNewPopup());
  ASSERT_TRUE(window_tracker->oauth_in_progress());

  // Spawn a regular browser window and verify it gets closed because it's not
  // authorized.
  BrowserWindowInterface* const unauthorized_regular_browser =
      CreateBrowserWindow(
          BrowserWindowCreateParams(BrowserWindowInterface::TYPE_NORMAL,
                                    profile(), /*from_user_gesture=*/true));
  ui_test_utils::BrowserDestroyedObserver destroyed_observer(
      unauthorized_regular_browser);

  // Wait for it to be minimized by OnBrowserActivated.
  // We need to ensure activation happens to trigger our logic.
  unauthorized_regular_browser->GetWindow()->Show();
  unauthorized_regular_browser->GetWindow()->Activate();
  destroyed_observer.Wait();

  // Verify it is closed (total browser count should be original + 1 (SWA +
  // OAuth popup)).
  ASSERT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(),
            original_browser_count + 1);
  EXPECT_FALSE(popup_browser->IsDeleteScheduled());

  // Cleanup authorized popup.
  ui_test_utils::BrowserDestroyedObserver popup_closed_observer(popup_browser);
  window_tracker->set_oauth_in_progress(false, nullptr);
  ui_test_utils::NavigateToURLWithDisposition(
      popup_browser,
      embedded_test_server()->GetURL(kTabUrlRedirectHost,
                                     "/redirect?code=secret"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);
  popup_closed_observer.Wait();
  EXPECT_TRUE(window_tracker->CanOpenNewPopup());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       OAuthStartedInTheMiddleOfAnotherOauthProcess) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  // Spawn tab for testing purposes.
  const GURL url_1 = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(
      window_id, url_1, ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Before we simulate an oauth flow within the same popup, we set oauth to be
  // in process.
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetInstance()->GetForBrowserContext(
          profile());
  window_tracker->set_oauth_in_progress(true, nullptr);

  // Spawn popup and simulate oauth request.
  //
  // In order to successfully simulate an oauth flow, we use an interceptor to
  // allow referencing arbitrary paths on the origin used to process the oauth
  // request without worrying that the corresponding test files exist.
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.GetHost() == kTabUrlRedirectHost) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/title2.html", params->client.get());
          return true;
        }
        // Not handled by us.
        return false;
      }));

  const size_t original_browser_count =
      GlobalBrowserCollection::GetInstance()->GetSize();
  NavigateParams navigate_params(
      boca_app_browser,
      embedded_test_server()->GetURL(
          kTabUrlRedirectHost,
          "/authenticate?client_id=123&response_type=code"),
      ui::PAGE_TRANSITION_LINK);
  navigate_params.disposition = WindowOpenDisposition::NEW_POPUP;
  navigate_params.window_action = NavigateParams::WindowAction::kShowWindow;
  ui_test_utils::NavigateToURL(&navigate_params);
  BrowserWindowInterface* const popup_browser = navigate_params.browser;

  ui_test_utils::BrowserActivationWaiter popup_activation_waiter(popup_browser);
  popup_activation_waiter.WaitForActivation();
  ASSERT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(),
            original_browser_count + 1);
  EXPECT_FALSE(popup_browser->IsDeleteScheduled());
  EXPECT_FALSE(window_tracker->CanOpenNewPopup());

  // The oauth popup in reality should close once the login flow is complete.
  // This is normally done through a redirect with an auto close window query,
  // but we simulate this in the test.
  ui_test_utils::BrowserDestroyedObserver popup_closed_observer(popup_browser);
  window_tracker->set_oauth_in_progress(false, nullptr);
  ui_test_utils::NavigateToURLWithDisposition(
      popup_browser,
      embedded_test_server()->GetURL(kTabUrlRedirectHost,
                                     "/redirect?code=secret"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);
  popup_closed_observer.Wait();
  EXPECT_TRUE(window_tracker->CanOpenNewPopup());
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       SuccessNavigationProceedsWithRedirects) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  const GURL url_1 = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(
      window_id, url_1, ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Trigger a redirect chain and verify it goes through.
  const GURL url_1_front_subdomain =
      embedded_test_server()->GetURL(kTabUrl1FrontSubDomainHost, "/");
  const GURL redirect_url = embedded_test_server()->GetURL(
      kTabUrlRedirectHost, "/server-redirect?" + url_1_front_subdomain.spec());
  const GURL url_1_redirect = embedded_test_server()->GetURL(
      kTabUrl1Host, "/server-redirect?" + redirect_url.spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, url_1_redirect));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_front_subdomain);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       SuccessNavigationProceedsWithClientRedirects) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  const GURL url_1 = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(
      window_id, url_1, ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Trigger a client redirect and verify it goes through.
  const GURL url_1_front_subdomain =
      embedded_test_server()->GetURL(kTabUrl1FrontSubDomainHost, "/");
  const GURL url_1_redirect = embedded_test_server()->GetURL(
      kTabUrl1Host, "/client-redirect?" + url_1_front_subdomain.spec());
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(boca_app_browser,
                                                            url_1_redirect, 2);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_front_subdomain);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       CloseNewTabWhenUrlNavigationBlocked) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  const GURL url_1 = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(window_id, url_1,
                             ::boca::LockedNavigationOptions::BLOCK_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Spawn child tab with a disallowed URL and verify the new tab is closed
  // subsequently.
  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
  ASSERT_TRUE(content::ExecJs(
      tab_strip_model->GetActiveWebContents(),
      content::JsReplace("window.open($1, '_blank');", different_domain_url)));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(tab_strip_model->count(), 2);
  VerifyUrlBlockedToastShown(/*was-toast_shown=*/true);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       BackForwardReloadNavigationSuccess) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  const GURL url_1 =
      embedded_test_server()->GetURL(kTabUrl1Host, "/title1.html");
  CreateBackgroundTabAndWait(
      window_id, url_1, ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Navigate to an accepted URL to test content navigation behavior.
  const GURL url_1_front_subdomain = embedded_test_server()->GetURL(
      kTabUrl1FrontSubDomainHost, "/title2.html");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, url_1_front_subdomain));
  ASSERT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            url_1_front_subdomain);
  WaitForUrlBlocklistUpdate();

  // Back navigation.
  auto* const active_web_contents = tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(chrome::CanGoBack(active_web_contents));
  chrome::GoBack(active_web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(active_web_contents));
  EXPECT_EQ(active_web_contents->GetLastCommittedURL(), url_1);

  // Forward navigation.
  ASSERT_TRUE(chrome::CanGoForward(active_web_contents));
  chrome::GoForward(active_web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(active_web_contents));
  EXPECT_EQ(active_web_contents->GetLastCommittedURL(), url_1_front_subdomain);

  // Reload navigation.
  ASSERT_TRUE(chrome::CanReload(boca_app_browser));
  content::TestNavigationObserver navigation_observer(active_web_contents);
  chrome::Reload(boca_app_browser, WindowOpenDisposition::CURRENT_TAB);
  navigation_observer.WaitForNavigationFinished();
  EXPECT_EQ(active_web_contents->GetLastCommittedURL(), url_1_front_subdomain);
}

IN_PROC_BROWSER_TEST_F(
    OnTaskLockedSessionNavigationThrottleInteractiveUITest,
    BackForwardReloadNavigationWithOneLevelDeepNavRestriction) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  const GURL url_1 =
      embedded_test_server()->GetURL(kTabUrl1Host, "/title1.html");
  CreateBackgroundTabAndWait(
      window_id, url_1, ::boca::LockedNavigationOptions::LIMITED_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Navigate to an accepted URL to test content navigation behavior.
  const GURL url_1_front_subdomain = embedded_test_server()->GetURL(
      kTabUrl1FrontSubDomainHost, "/title2.html");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, url_1_front_subdomain));
  ASSERT_EQ(tab_strip_model->GetActiveWebContents()->GetVisibleURL(),
            url_1_front_subdomain);
  WaitForUrlBlocklistUpdate();

  // Back navigation.
  auto* const active_web_contents = tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(chrome::CanGoBack(active_web_contents));
  chrome::GoBack(active_web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(active_web_contents));
  EXPECT_EQ(active_web_contents->GetLastCommittedURL(), url_1);

  // Forward navigation.
  ASSERT_TRUE(chrome::CanGoForward(active_web_contents));
  chrome::GoForward(active_web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(active_web_contents));
  EXPECT_EQ(active_web_contents->GetLastCommittedURL(), url_1_front_subdomain);

  // Reload navigation.
  ASSERT_TRUE(chrome::CanReload(boca_app_browser));
  content::TestNavigationObserver navigation_observer(active_web_contents);
  chrome::Reload(boca_app_browser, WindowOpenDisposition::CURRENT_TAB);
  navigation_observer.WaitForNavigationFinished();
  EXPECT_EQ(active_web_contents->GetLastCommittedURL(), url_1_front_subdomain);
}

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleInteractiveUITest,
                       BlockPostMethodNavigationRequests) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  const GURL url_1 = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  CreateBackgroundTabAndWait(
      window_id, url_1, ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Submit post request and verify it gets blocked.
  NavigateParams navigate_params(
      boca_app_browser,
      embedded_test_server()->GetURL(kTabUrl1Host, "/echotitle"),
      ui::PAGE_TRANSITION_LINK);
  navigate_params.disposition = WindowOpenDisposition::CURRENT_TAB;
  navigate_params.post_data =
      network::ResourceRequestBody::CreateFromCopyOfBytes(
          base::as_byte_span("TestContent"));
  ui_test_utils::NavigateToURL(&navigate_params);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1);
}

class OnTaskLockedSessionNavigationThrottleWorkspaceNavigationInteractiveUITest
    : public OnTaskLockedSessionNavigationThrottleInteractiveUITestBase {
 protected:
  // Override the embedded test server accessor so we use the HTTPS server that
  // we set up for the test.
  net::EmbeddedTestServer* embedded_test_server() { return &https_server_; }

  void SetUpOnMainThread() override {
    OnTaskLockedSessionNavigationThrottleInteractiveUITestBase::
        SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    embedded_test_server()->SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  // In order to prevent HTTPS redirects with workspace URL navigation, we set
  // up a test server that supports SSL.
  net::EmbeddedTestServer https_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(
    OnTaskLockedSessionNavigationThrottleWorkspaceNavigationInteractiveUITest,
    AllowAndBlockUrlsForWorkspaceNavRestriction) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabGoogleHost, "/"),
      ::boca::LockedNavigationOptions::WORKSPACE_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Navigate to URLs that should be allowed to go through.
  const GURL google_docs_url =
      embedded_test_server()->GetURL(kTabGoogleDocsHost, "/some-doc");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, google_docs_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            google_docs_url);
  WaitForUrlBlocklistUpdate();

  const GURL google_search_url =
      embedded_test_server()->GetURL(kTabGoogleHost, "/search?q=test");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, google_search_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            google_search_url);
  WaitForUrlBlocklistUpdate();
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/false);

  // Navigate to URLs that should be blocked.
  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, different_domain_url));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);
}

IN_PROC_BROWSER_TEST_F(
    OnTaskLockedSessionNavigationThrottleWorkspaceNavigationInteractiveUITest,
    AllowAndBlockUrlsForWorkspaceNavRestrictionOnNewTab) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabGoogleHost, "/"),
      ::boca::LockedNavigationOptions::WORKSPACE_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Spawn child tab to simulate opening a new link.
  const GURL google_docs_url =
      embedded_test_server()->GetURL(kTabGoogleDocsHost, "/some-doc");
  SpawnChildTabWithURL(boca_app_browser, google_docs_url);
  content::RunAllTasksUntilIdle();
  ASSERT_EQ(tab_strip_model->count(), 3);
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            google_docs_url);
  WaitForUrlBlocklistUpdate();
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/false);

  // Verify we cannot navigate to other URLs.
  const GURL different_domain_url =
      embedded_test_server()->GetURL(kTabUrl2Host, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, different_domain_url));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            different_domain_url);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);
}

IN_PROC_BROWSER_TEST_F(
    OnTaskLockedSessionNavigationThrottleWorkspaceNavigationInteractiveUITest,
    BlockWorkspaceUrlsForOtherNavRestriction) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl1Host, "/"),
      ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Verify sub-domains are allowed.
  const GURL url_1_front_subdomain =
      embedded_test_server()->GetURL(kTabUrl1FrontSubDomainHost, "/");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, url_1_front_subdomain));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1_front_subdomain);
  WaitForUrlBlocklistUpdate();
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/false);

  // Verify workspace URL navigations are blocked.
  const GURL google_docs_url =
      embedded_test_server()->GetURL(kTabGoogleDocsHost, "/some-doc");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, google_docs_url));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            google_docs_url);

  const GURL google_url = embedded_test_server()->GetURL(kTabGoogleHost, "/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, google_url));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            google_url);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);
}

IN_PROC_BROWSER_TEST_F(
    OnTaskLockedSessionNavigationThrottleWorkspaceNavigationInteractiveUITest,
    BlockRedirectsCreatedAsSeparateNavigationRequests) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl1Host, "/"),
      ::boca::LockedNavigationOptions::LIMITED_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Simulate URL redirect as a separate navigation request.
  const GURL google_redirect_url = embedded_test_server()->GetURL(
      kTabGoogleHost, "/url?q=https://www.foo.com");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, google_redirect_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            google_redirect_url);

  const GURL url_1 = embedded_test_server()->GetURL(kTabUrl1Host, "/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, url_1));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            url_1);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/true);
}

IN_PROC_BROWSER_TEST_F(
    OnTaskLockedSessionNavigationThrottleWorkspaceNavigationInteractiveUITest,
    AllowOneLevelDeepNavigationWithGoogleCaptcha) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  BrowserWindowInterface* const boca_app_browser =
      FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca::OnTaskLockedController::From(boca_app_browser)
                  ->is_locked_for_on_task());

  // Set up window tracker to track the app window.
  const SessionID window_id = boca_app_browser->GetSessionID();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Spawn tab for testing purposes.
  CreateBackgroundTabAndWait(
      window_id, embedded_test_server()->GetURL(kTabUrl1Host, "/"),
      ::boca::LockedNavigationOptions::LIMITED_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->GetTabStripModel();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  WaitForUrlBlocklistUpdate();

  // Navigate to Google search and simulate Captcha redirect.
  const GURL google_search_url =
      embedded_test_server()->GetURL(kTabGoogleHost, "/search?q=test");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, google_search_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            google_search_url);

  const GURL google_captcha_url =
      embedded_test_server()->GetURL(kTabGoogleHost, "/sorry/index");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(boca_app_browser, google_captcha_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            google_captcha_url);
  VerifyUrlBlockedToastShown(/*toast_was_shown=*/false);
}

}  // namespace
}  // namespace ash
