// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_pod_controller_impl.h"

#include <vector>

#include "ash/boca/on_task/on_task_pod_controller.h"
#include "ash/boca/on_task/on_task_pod_utils.h"
#include "ash/boca/on_task/on_task_pod_view.h"
#include "ash/constants/ash_features.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/window_pin_util.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using ::testing::IsNull;
using ::testing::NotNull;

namespace ash {
namespace {

class OnTaskPodControllerImplBrowserTestBase : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ash::SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();
    system_web_app_manager_ =
        std::make_unique<boca::OnTaskSystemWebAppManagerImpl>(profile());
    host_resolver()->AddRule("*", "127.0.0.1");
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

  LockedSessionWindowTracker* window_tracker() {
    return LockedSessionWindowTrackerFactory::GetInstance()
        ->GetForBrowserContext(profile());
  }

  ash::OnTaskPodControllerImpl* on_task_pod_controller() {
    return static_cast<ash::OnTaskPodControllerImpl*>(
        window_tracker()->on_task_pod_controller());
  }

  boca::OnTaskSystemWebAppManagerImpl* system_web_app_manager() {
    return system_web_app_manager_.get();
  }

 private:
  std::unique_ptr<boca::OnTaskSystemWebAppManagerImpl> system_web_app_manager_;
};

class OnTaskPodControllerImplSetupBrowserTest
    : public OnTaskPodControllerImplBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  OnTaskPodControllerImplSetupBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features{
        features::kBoca, features::kBocaConsumer,
        features::kOnDeviceSpeechRecognition};
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsOnTaskPodEnabled()) {
      enabled_features.push_back(features::kBocaOnTaskPod);
    } else {
      disabled_features.push_back(features::kBocaOnTaskPod);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool IsOnTaskPodEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(OnTaskPodControllerImplSetupBrowserTest,
                       PodSetupWithFeatureFlag) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. Verify that the pod is set
  // up only when the feature flag is enabled.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  if (IsOnTaskPodEnabled()) {
    ASSERT_THAT(on_task_pod_controller(), NotNull());
    views::Widget* const pod_widget =
        on_task_pod_controller()->GetPodWidgetForTesting();
    ASSERT_THAT(pod_widget, NotNull());
    EXPECT_TRUE(pod_widget->IsVisible());
    EXPECT_TRUE(pod_widget->GetContentsView()->GetVisible());
  } else {
    EXPECT_THAT(on_task_pod_controller(), IsNull());
  }
}

INSTANTIATE_TEST_SUITE_P(OnTaskPodControllerImplSetupBrowserTests,
                         OnTaskPodControllerImplSetupBrowserTest,
                         ::testing::Bool());

class OnTaskPodControllerImplBrowserTest
    : public OnTaskPodControllerImplBrowserTestBase {
 protected:
  OnTaskPodControllerImplBrowserTest() {
    // Enable Boca and consumer experience for testing purposes. This is used
    // to set up the Boca SWA for OnTask.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kBoca, features::kBocaConsumer,
                              features::kBocaOnTaskPod,
                              features::kOnDeviceSpeechRecognition},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    OnTaskPodControllerImplBrowserTestBase::SetUpOnMainThread();
    embedded_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(embedded_test_server()->Start());
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       DestroyPodOnWindowClose) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  boca_app_browser->tab_strip_model()->CloseAllTabs();
  content::RunAllTasksUntilIdle();
  EXPECT_THAT(on_task_pod_controller(), IsNull());
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       HidePodWhenWindowHidden) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Hide window and verify the pod widget also gets hidden.
  boca_app_browser->window()->GetNativeWindow()->Hide();
  EXPECT_FALSE(on_task_pod_controller()->GetPodWidgetForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       ShowPodWhenWindowShown) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Hide window and verify the pod widget also gets hidden.
  boca_app_browser->window()->GetNativeWindow()->Hide();
  ASSERT_FALSE(on_task_pod_controller()->GetPodWidgetForTesting()->IsVisible());

  // Show the window and verify the pod is shown.
  boca_app_browser->window()->GetNativeWindow()->Show();
  EXPECT_TRUE(on_task_pod_controller()->GetPodWidgetForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       DestroyPodOnWindowTrackerReset) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  window_tracker()->InitializeBrowserInfoForTracking(nullptr);
  EXPECT_THAT(on_task_pod_controller(), IsNull());
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       BackAndForwardButtonDisabled) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Spawn a new tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL tab_url = embedded_test_server()->GetURL("/title1.html");
  CreateBackgroundTabAndWait(
      window_id, tab_url, ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_FALSE(on_task_pod_controller()->CanNavigateToPreviousPage());
  ASSERT_FALSE(on_task_pod_controller()->CanNavigateToNextPage());

  tab_strip_model->ActivateTabAt(0);
  ASSERT_FALSE(on_task_pod_controller()->CanNavigateToPreviousPage());
  ASSERT_FALSE(on_task_pod_controller()->CanNavigateToNextPage());
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       ForwardButtonDisabled) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Spawn a new tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL tab_url = embedded_test_server()->GetURL("/title1.html");
  CreateBackgroundTabAndWait(window_id, tab_url,
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);

  // Navigate the tab to a new page.
  const GURL new_url = embedded_test_server()->GetURL("/new_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, new_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            new_url);
  ASSERT_TRUE(on_task_pod_controller()->CanNavigateToPreviousPage());
  ASSERT_FALSE(on_task_pod_controller()->CanNavigateToNextPage());
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest, BackButtonDisabled) {
  base::UserActionTester actions;

  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Spawn a new tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL tab_url = embedded_test_server()->GetURL("/title1.html");
  CreateBackgroundTabAndWait(window_id, tab_url,
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);

  // Navigate the tab to a new page and then navigate back.
  const GURL new_url = embedded_test_server()->GetURL("/new_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, new_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            new_url);
  ASSERT_TRUE(on_task_pod_controller()->CanNavigateToPreviousPage());
  on_task_pod_controller()->MaybeNavigateToPreviousPage();
  content::WaitForLoadStop(tab_strip_model->GetActiveWebContents());
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            tab_url);
  ASSERT_FALSE(on_task_pod_controller()->CanNavigateToPreviousPage());
  ASSERT_TRUE(on_task_pod_controller()->CanNavigateToNextPage());

  EXPECT_EQ(
      actions.GetActionCount(boca::kBocaOnTaskActionOfStudentNavigateBack), 1);
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       NavigateBackAndForward) {
  base::UserActionTester actions;

  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Spawn a new tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL tab_url = embedded_test_server()->GetURL("/title1.html");
  CreateBackgroundTabAndWait(window_id, tab_url,
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);

  // Navigate the tab to a new page.
  const GURL new_url = embedded_test_server()->GetURL("/new_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, new_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            new_url);

  // Navigate back to the previous page.
  ASSERT_TRUE(on_task_pod_controller()->CanNavigateToPreviousPage());
  on_task_pod_controller()->MaybeNavigateToPreviousPage();
  content::WaitForLoadStop(tab_strip_model->GetActiveWebContents());
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            tab_url);

  // Navigate forward to the next page.
  ASSERT_TRUE(on_task_pod_controller()->CanNavigateToNextPage());
  on_task_pod_controller()->MaybeNavigateToNextPage();
  content::WaitForLoadStop(tab_strip_model->GetActiveWebContents());
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            new_url);

  EXPECT_EQ(
      actions.GetActionCount(boca::kBocaOnTaskActionOfStudentNavigateBack), 1);
  EXPECT_EQ(
      actions.GetActionCount(boca::kBocaOnTaskActionOfStudentNavigateForward),
      1);
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       DifferentTabsWithDifferentPod) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Spawn two new tabs for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL tab_url_1 = embedded_test_server()->GetURL("/title1.html");
  CreateBackgroundTabAndWait(window_id, tab_url_1,
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  const GURL tab_url_2 = embedded_test_server()->GetURL("/title2.html");
  CreateBackgroundTabAndWait(window_id, tab_url_2,
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 3);

  // Navigate the first tab to a new page.
  tab_strip_model->ActivateTabAt(1);
  const GURL new_url = embedded_test_server()->GetURL("/new_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(boca_app_browser, new_url));
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            new_url);
  ASSERT_TRUE(on_task_pod_controller()->CanNavigateToPreviousPage());
  ASSERT_FALSE(on_task_pod_controller()->CanNavigateToNextPage());

  // The second tab should have both back and forward buttons disabled.
  tab_strip_model->ActivateTabAt(2);
  ASSERT_FALSE(on_task_pod_controller()->CanNavigateToPreviousPage());
  ASSERT_FALSE(on_task_pod_controller()->CanNavigateToNextPage());
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest, ReloadCurrentTab) {
  base::UserActionTester actions;

  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Spawn a new tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL tab_url = embedded_test_server()->GetURL("/title1.html");
  CreateBackgroundTabAndWait(
      window_id, tab_url, ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);

  on_task_pod_controller()->ReloadCurrentPage();
  content::WaitForLoadStop(tab_strip_model->GetActiveWebContents());
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            tab_url);

  tab_strip_model->ActivateTabAt(0);
  on_task_pod_controller()->ReloadCurrentPage();
  content::WaitForLoadStop(tab_strip_model->GetActiveWebContents());
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            tab_url);

  EXPECT_EQ(actions.GetActionCount(boca::kBocaOnTaskActionOfStudentReloadPage),
            2);
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       DisablePinTabStripFunctionalityWhenUnlocked) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Spawn a new tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL tab_url = embedded_test_server()->GetURL("/title1.html");
  CreateBackgroundTabAndWait(window_id, tab_url,
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  EXPECT_FALSE(on_task_pod_controller()->CanToggleTabStripVisibility());
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       EnablePinTabStripFunctionalityWhenLocked) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Spawn a new tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL tab_url = embedded_test_server()->GetURL("/title1.html");
  CreateBackgroundTabAndWait(window_id, tab_url,
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  EXPECT_TRUE(on_task_pod_controller()->CanToggleTabStripVisibility());
}

IN_PROC_BROWSER_TEST_F(
    OnTaskPodControllerImplBrowserTest,
    EnablePinTabStripFunctionalityWhenLockedAndPinAnotherWindow) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Spawn a new tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL tab_url = embedded_test_server()->GetURL("/title1.html");
  CreateBackgroundTabAndWait(window_id, tab_url,
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  ASSERT_TRUE(on_task_pod_controller()->CanToggleTabStripVisibility());

  // Pin another browser.
  Browser* const new_browser = browser();
  chrome::NewTab(new_browser);
  aura::Window* const new_window = new_browser->window()->GetNativeWindow();
  PinWindow(new_window, /*trusted=*/true);
  ASSERT_TRUE(on_task_pod_controller()->CanToggleTabStripVisibility());
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest, HidePodWhenPaused) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Pause the app and verify the pod widget also gets hidden.
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  system_web_app_manager()->SetPauseStateForSystemWebAppWindow(/*paused=*/true,
                                                               window_id);
  ASSERT_THAT(on_task_pod_controller(), NotNull());
  EXPECT_FALSE(on_task_pod_controller()->GetPodWidgetForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       ShowPodWhenUnpaused) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Pause the app and verify the pod widget also gets hidden.
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  system_web_app_manager()->SetPauseStateForSystemWebAppWindow(/*paused=*/true,
                                                               window_id);
  ASSERT_THAT(on_task_pod_controller(), NotNull());
  EXPECT_FALSE(on_task_pod_controller()->GetPodWidgetForTesting()->IsVisible());

  // Unpause the app and verify the pod widget also gets shown.
  system_web_app_manager()->SetPauseStateForSystemWebAppWindow(/*paused=*/false,
                                                               window_id);
  ASSERT_THAT(on_task_pod_controller(), NotNull());
  EXPECT_TRUE(on_task_pod_controller()->GetPodWidgetForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       ShowAndHideTabStripWhenTogglePinTabStripButton) {
  base::UserActionTester actions;

  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Spawn a new tab for testing purposes.
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  const GURL tab_url = embedded_test_server()->GetURL("/title1.html");
  CreateBackgroundTabAndWait(window_id, tab_url,
                             ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  ASSERT_EQ(tab_strip_model->count(), 2);
  ASSERT_TRUE(on_task_pod_controller()->CanToggleTabStripVisibility());

  on_task_pod_controller()->ToggleTabStripVisibility(true, true);
  EXPECT_THAT(on_task_pod_controller()->GetTabStripRevealLockForTesting(),
              NotNull());
  on_task_pod_controller()->ToggleTabStripVisibility(false, true);
  EXPECT_THAT(on_task_pod_controller()->GetTabStripRevealLockForTesting(),
              IsNull());

  EXPECT_EQ(actions.GetActionCount(
                boca::kBocaOnTaskActionOfStudentToggleTabStripVisibility),
            2);
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       RepositionPodOnWindowBoundsChanged) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Verify initial pod position.
  ASSERT_EQ(on_task_pod_controller()->GetSnapLocationForTesting(),
            OnTaskPodSnapLocation::kTopLeft);
  views::Widget* const on_task_pod_widget =
      on_task_pod_controller()->GetPodWidgetForTesting();
  const gfx::Rect boca_app_browser_bounds =
      on_task_pod_widget->parent()->GetWindowBoundsInScreen();
  const int boca_app_browser_frame_header_height =
      boca::GetFrameHeaderHeight(on_task_pod_widget->parent());
  EXPECT_EQ(on_task_pod_widget->GetWindowBoundsInScreen().origin(),
            gfx::Point(boca_app_browser_bounds.x() + kPodVerticalBorder,
                       boca_app_browser_bounds.y() +
                           boca_app_browser_frame_header_height +
                           kPodHorizontalBorder));

  // Update browser window bounds and verify the new position of the pod.
  const gfx::Rect new_boca_app_browser_bounds(
      boca_app_browser_bounds.x() + 1, boca_app_browser_bounds.y() + 1,
      boca_app_browser_bounds.width() + 1,
      boca_app_browser_bounds.height() + 1);
  on_task_pod_widget->parent()->SetBounds(new_boca_app_browser_bounds);
  EXPECT_EQ(on_task_pod_widget->GetWindowBoundsInScreen().origin(),
            gfx::Point(new_boca_app_browser_bounds.x() + kPodVerticalBorder,
                       new_boca_app_browser_bounds.y() +
                           boca_app_browser_frame_header_height +
                           kPodHorizontalBorder));
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest, SetPodSnapLocation) {
  base::UserActionTester actions;

  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Verify initial pod snap location with position.
  ASSERT_EQ(on_task_pod_controller()->GetSnapLocationForTesting(),
            OnTaskPodSnapLocation::kTopLeft);
  views::Widget* const on_task_pod_widget =
      on_task_pod_controller()->GetPodWidgetForTesting();
  const gfx::Rect boca_app_browser_bounds =
      on_task_pod_widget->parent()->GetWindowBoundsInScreen();
  const int boca_app_browser_frame_header_height =
      boca::GetFrameHeaderHeight(on_task_pod_widget->parent());
  EXPECT_EQ(on_task_pod_widget->GetWindowBoundsInScreen().origin(),
            gfx::Point(boca_app_browser_bounds.x() + kPodVerticalBorder,
                       boca_app_browser_bounds.y() +
                           boca_app_browser_frame_header_height +
                           kPodHorizontalBorder));

  // Update pod snap location and verify its new position.
  on_task_pod_controller()->SetSnapLocation(OnTaskPodSnapLocation::kTopRight);
  ASSERT_EQ(on_task_pod_controller()->GetSnapLocationForTesting(),
            OnTaskPodSnapLocation::kTopRight);
  EXPECT_EQ(on_task_pod_widget->GetWindowBoundsInScreen().origin(),
            gfx::Point(boca_app_browser_bounds.right() -
                           on_task_pod_widget->GetContentsView()
                               ->GetPreferredSize()
                               .width() -
                           kPodVerticalBorder,
                       boca_app_browser_bounds.y() +
                           boca_app_browser_frame_header_height +
                           kPodHorizontalBorder));

  // Update pod snap location to its initial value and verify its position is
  // reset.
  on_task_pod_controller()->SetSnapLocation(OnTaskPodSnapLocation::kTopLeft);
  ASSERT_EQ(on_task_pod_controller()->GetSnapLocationForTesting(),
            OnTaskPodSnapLocation::kTopLeft);
  EXPECT_EQ(on_task_pod_widget->GetWindowBoundsInScreen().origin(),
            gfx::Point(boca_app_browser_bounds.x() + kPodVerticalBorder,
                       boca_app_browser_bounds.y() +
                           boca_app_browser_frame_header_height +
                           kPodHorizontalBorder));

  EXPECT_EQ(actions.GetActionCount(
                boca::kBocaOnTaskActionOfStudentSetSnapLocationToRight),
            1);
  EXPECT_EQ(actions.GetActionCount(
                boca::kBocaOnTaskActionOfStudentSetSnapLocationToLeft),
            1);
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       RepositionPodWhenSnapLocationAndLocked) {
  base::UserActionTester actions;

  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());

  // Verify initial pod position.
  ASSERT_EQ(on_task_pod_controller()->GetSnapLocationForTesting(),
            OnTaskPodSnapLocation::kTopLeft);
  views::Widget* const on_task_pod_widget =
      on_task_pod_controller()->GetPodWidgetForTesting();
  const gfx::Rect boca_app_browser_bounds =
      on_task_pod_widget->parent()->GetWindowBoundsInScreen();
  const int boca_app_browser_frame_header_height =
      boca::GetFrameHeaderHeight(on_task_pod_widget->parent());
  EXPECT_EQ(on_task_pod_widget->GetWindowBoundsInScreen().origin(),
            gfx::Point(boca_app_browser_bounds.x() + kPodVerticalBorder,
                       boca_app_browser_bounds.y() +
                           boca_app_browser_frame_header_height +
                           kPodHorizontalBorder));

  // Update pod snap location and verify its new position.
  on_task_pod_controller()->SetSnapLocation(OnTaskPodSnapLocation::kTopRight);
  ASSERT_EQ(on_task_pod_controller()->GetSnapLocationForTesting(),
            OnTaskPodSnapLocation::kTopRight);
  EXPECT_EQ(on_task_pod_widget->GetWindowBoundsInScreen().origin(),
            gfx::Point(boca_app_browser_bounds.right() -
                           on_task_pod_widget->GetContentsView()
                               ->GetPreferredSize()
                               .width() -
                           kPodVerticalBorder,
                       boca_app_browser_bounds.y() +
                           boca_app_browser_frame_header_height +
                           kPodHorizontalBorder));

  // Pin the window and verify the new position of the pod.
  system_web_app_manager()->SetPinStateForSystemWebAppWindow(/*pinned=*/true,
                                                             window_id);
  ASSERT_THAT(on_task_pod_controller(), NotNull());
  const gfx::Rect new_boca_app_browser_bounds =
      on_task_pod_widget->parent()->GetWindowBoundsInScreen();
  EXPECT_EQ(on_task_pod_widget->GetWindowBoundsInScreen().origin(),
            gfx::Point(new_boca_app_browser_bounds.right() -
                           on_task_pod_widget->GetContentsView()
                               ->GetPreferredSize()
                               .width() -
                           kPodVerticalBorder,
                       new_boca_app_browser_bounds.y() +
                           boca_app_browser_frame_header_height +
                           kPodHorizontalBorder));

  EXPECT_EQ(actions.GetActionCount(
                boca::kBocaOnTaskActionOfStudentSetSnapLocationToRight),
            1);
}

IN_PROC_BROWSER_TEST_F(OnTaskPodControllerImplBrowserTest,
                       FocusShouldEscapePod) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is when the OnTask pod
  // is set up.
  const SessionID window_id = boca_app_browser->session_id();
  ASSERT_TRUE(window_id.is_valid());
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});
  ASSERT_THAT(on_task_pod_controller(), NotNull());
  views::Widget* const on_task_pod_widget =
      on_task_pod_controller()->GetPodWidgetForTesting();
  OnTaskPodView* const on_task_pod_view =
      static_cast<OnTaskPodView*>(on_task_pod_widget->GetContentsView());
  views::FocusManager* const focus_manager =
      on_task_pod_widget->GetFocusManager();

  // Set focus to the last focusable element, focus should move outside the pod
  // widget when advancing focus.
  focus_manager->SetFocusedView(
      on_task_pod_view->reload_tab_button_for_testing());
  ASSERT_EQ(on_task_pod_view->reload_tab_button_for_testing(),
            focus_manager->GetFocusedView());
  focus_manager->AdvanceFocus(/*reverse=*/false);
  EXPECT_NE(on_task_pod_view->dock_left_button_for_testing(),
            focus_manager->GetFocusedView());

  // Set focus to the first focusable element, focus should move outside the pod
  // widget when reverse advancing focus.
  focus_manager->SetFocusedView(
      on_task_pod_view->dock_left_button_for_testing());
  ASSERT_EQ(on_task_pod_view->dock_left_button_for_testing(),
            focus_manager->GetFocusedView());
  focus_manager->AdvanceFocus(/*reverse=*/true);
  EXPECT_NE(on_task_pod_view->reload_tab_button_for_testing(),
            focus_manager->GetFocusedView());
}

}  // namespace
}  // namespace ash
