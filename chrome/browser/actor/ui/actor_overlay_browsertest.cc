// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/ui/actor_overlay_window_controller.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

namespace actor::ui {
namespace {
using actor::mojom::ActionResultPtr;
using base::test::TestFuture;

class ActorOverlayTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi, {{features::kGlicActorUiOverlayName, "true"}});
    InProcessBrowserTest::SetUp();
  }

  bool IsActorOverlayVisible(Browser* browser) const {
    return browser->GetBrowserView()
        .GetActiveContentsContainerView()
        ->actor_overlay_view()
        ->GetVisible();
  }

  unsigned int NumActorOverlayChildren(Browser* browser) const {
    return browser->GetBrowserView()
        .GetActiveContentsContainerView()
        ->actor_overlay_view()
        ->children()
        .size();
  }

  bool IsActorOverlayChildVisible(Browser* browser) const {
    EXPECT_EQ(NumActorOverlayChildren(browser), 1u)
        << "Child 0 is not present or extra children are present";
    return browser->GetBrowserView()
        .GetActiveContentsContainerView()
        ->actor_overlay_view()
        ->children()[0]
        ->GetVisible();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorOverlayTest, PageLoadsWhenFeatureOn) {
  GURL kUrl(chrome::kChromeUIActorOverlayURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(web_contents->GetLastCommittedURL(), kUrl);
  EXPECT_FALSE(web_contents->IsCrashed());
  EXPECT_EQ(web_contents->GetTitle(), u"Actor Overlay");
}

// Verifies that the ActorOverlayWindowController and Actor Ui Tab Controller
// should only exist for normal browser windows.
IN_PROC_BROWSER_TEST_F(ActorOverlayTest, ControllerExistsForNormalBrowsers) {
  Profile* const profile = browser()->profile();

  // Normal browser window
  Browser* const normal_browser = browser();
  ASSERT_NE(ActorOverlayWindowController::From(normal_browser), nullptr);
  ASSERT_NE(normal_browser->browser_window_features()
                ->tab_strip_model()
                ->GetActiveTab()
                ->GetTabFeatures()
                ->actor_ui_tab_controller(),
            nullptr);

  // Popup window
  Browser* const popup_browser = CreateBrowserForPopup(profile);
  ASSERT_EQ(ActorOverlayWindowController::From(popup_browser), nullptr);
  ASSERT_EQ(popup_browser->browser_window_features()
                ->tab_strip_model()
                ->GetActiveTab()
                ->GetTabFeatures()
                ->actor_ui_tab_controller(),
            nullptr);

  // App window
  Browser* const app_browser = CreateBrowserForApp("test_app_name", profile);
  ASSERT_EQ(ActorOverlayWindowController::From(app_browser), nullptr);
  ASSERT_EQ(app_browser->browser_window_features()
                ->tab_strip_model()
                ->GetActiveTab()
                ->GetTabFeatures()
                ->actor_ui_tab_controller(),
            nullptr);

  // Picture-in-Picture window
  Browser* const pip_browser =
      Browser::Create(Browser::CreateParams::CreateForPictureInPicture(
          "test_app_name", false, profile, false));
  ASSERT_EQ(ActorOverlayWindowController::From(pip_browser), nullptr);
  // Tab Interface is null for Picture-in-Picture windows, so we don't test the
  // tab controller's existence.

  // DevTools window
  Browser* const devtools_browser =
      Browser::Create(Browser::CreateParams::CreateForDevTools(profile));
  ASSERT_EQ(ActorOverlayWindowController::From(devtools_browser), nullptr);
  // Tab Interface is null for DevTools windows, so we don't test the tab
  // controller's existence.
}

// Testing the Actor Overlay Window Controller
IN_PROC_BROWSER_TEST_F(ActorOverlayTest, ViewLifecycleAndVisibility) {
  ActorOverlayWindowController* window_controller =
      ActorOverlayWindowController::From(browser());
  ASSERT_NE(window_controller, nullptr);

  // The main actor_overlay_view container should initially be hidden. It should
  // also have no children.
  EXPECT_FALSE(IsActorOverlayVisible(browser()));
  EXPECT_EQ(NumActorOverlayChildren(browser()), 0u);
  content::BrowserContext* browser_context =
      browser()->tab_strip_model()->GetActiveWebContents()->GetBrowserContext();

  // Add a new WebView, initially hidden.
  auto web_view = std::make_unique<views::WebView>(browser_context);
  web_view->SetVisible(false);
  raw_ptr<views::WebView> overlay_web_view =
      window_controller->AddChildWebView(std::move(web_view));
  ASSERT_NE(overlay_web_view, nullptr);

  // Verify container size and that it remains hidden because the child is
  // hidden.
  EXPECT_EQ(NumActorOverlayChildren(browser()), 1u);
  EXPECT_FALSE(IsActorOverlayVisible(browser()));

  // Make the added WebView visible, and update the container's visibility.
  overlay_web_view->SetVisible(true);
  window_controller->MaybeUpdateContainerVisibility();

  // Container view should now be visible.
  EXPECT_TRUE(IsActorOverlayVisible(browser()));
  std::unique_ptr<views::WebView> managed_overlay_web_view =
      window_controller->RemoveChildWebView(overlay_web_view);
  // The raw_ptr to the removed view is now invalid, so set it to nullptr.
  overlay_web_view = nullptr;

  // Confirm managed WebView is not null and the container should become hidden
  // again
  ASSERT_NE(managed_overlay_web_view, nullptr);
  EXPECT_FALSE(IsActorOverlayVisible(browser()));
  EXPECT_EQ(NumActorOverlayChildren(browser()), 0u);
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest, SendStartEventAndStopEvent) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabHandle tab_handle =
      browser()->tab_strip_model()->GetActiveTab()->GetHandle();
  TestFuture<void> future;
  ActorUiTabControllerInterface* controller = browser()
                                                  ->tab_strip_model()
                                                  ->GetActiveTab()
                                                  ->GetTabFeatures()
                                                  ->actor_ui_tab_controller();
  controller->SetCallbackForTesting(future.GetCallback());
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(StartingToActOnTab(tab_handle, TaskId(1)),
                           result.GetCallback());
  ExpectOkResult(result);
  // Ensure callback is done.
  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser()); }));
  EXPECT_TRUE(IsActorOverlayChildVisible(browser()));
  state_manager->OnUiEvent(StoppedActingOnTab(tab_handle));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsActorOverlayVisible(browser()); }));
  EXPECT_FALSE(IsActorOverlayChildVisible(browser()));
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest, OverlayHidesOnTabBackgrounding) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabHandle tab_handle =
      browser()->tab_strip_model()->GetActiveTab()->GetHandle();
  // Set up callback logic.
  TestFuture<void> future;
  ActorUiTabControllerInterface* controller = browser()
                                                  ->tab_strip_model()
                                                  ->GetActiveTab()
                                                  ->GetTabFeatures()
                                                  ->actor_ui_tab_controller();
  controller->SetCallbackForTesting(future.GetCallback());
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(StartingToActOnTab(tab_handle, TaskId(1)),
                           result.GetCallback());
  ExpectOkResult(result);
  // Ensure callback is done.
  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser()); }));
  EXPECT_TRUE(IsActorOverlayChildVisible(browser()));
  browser()->tab_strip_model()->AppendWebContents(
      content::WebContents::Create(content::WebContents::CreateParams(profile)),
      /*foreground=*/true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsActorOverlayVisible(browser()); }));
  EXPECT_FALSE(IsActorOverlayChildVisible(browser()));
  browser()->tab_strip_model()->ActivateTabAt(0);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser()); }));
  EXPECT_TRUE(IsActorOverlayChildVisible(browser()));
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest, RepeatedlyMoveTabBetweenWindows) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  // Initial tab setup: Create 3 tabs in the starting browser window.
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  tabs::TabInterface* tab_1 = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_NE(tab_1, nullptr);
  tabs::TabInterface* tab_2 =
      tabs::TabInterface::GetFromContents(&chrome::NewTab(browser()));
  ASSERT_NE(tab_2, nullptr);

  // Set up callback logic after tab_2 is created.
  TestFuture<void> future;
  ActorUiTabControllerInterface* controller =
      tab_2->GetTabFeatures()->actor_ui_tab_controller();
  controller->SetCallbackForTesting(future.GetCallback());

  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  tabs::TabInterface* tab_3 =
      tabs::TabInterface::GetFromContents(&chrome::NewTab(browser()));
  ASSERT_NE(tab_3, nullptr);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);
  // We have 3 tabs {0, 1, 2}, so we're moving the last tab to a new window
  chrome::MoveTabsToNewWindow(browser(), {2});
  // Get references to both browser windows after the move.
  Browser* browser_1 =
      BrowserWindow::FindBrowserWindowWithWebContents(tab_1->GetContents())
          ->AsBrowserView()
          ->browser();
  Browser* browser_2 =
      BrowserWindow::FindBrowserWindowWithWebContents(tab_3->GetContents())
          ->AsBrowserView()
          ->browser();
  ASSERT_EQ(browser_1->tab_strip_model()->count(), 2);
  ASSERT_EQ(browser_2->tab_strip_model()->count(), 1);
  // Start actor actuation on tab_2, which is in browser_1.
  // This should make the Actor Overlay visible in browser_1.
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(StartingToActOnTab(tab_2->GetHandle(), TaskId(1)),
                           result.GetCallback());
  ExpectOkResult(result);
  // Ensure callback is done.
  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser_1); }));
  // Loop to repeatedly move the actuated tab between the two windows.
  // This verifies the overlay's persistence and correct re-parenting across
  // window changes. The number of iterations (10) is arbitrary and can be
  // adjusted.
  Browser* source_browser;
  Browser* target_browser;
  for (int i = 0; i < 10; ++i) {
    // Determine current source and target browsers for the move.
    source_browser = (i % 2 == 0) ? browser_1 : browser_2;
    target_browser = (i % 2 == 0) ? browser_2 : browser_1;
    // Move tab_2, which is always added to the last index of the target
    // browser. In this test, it consistently moves from index 1 to index 1 in
    // the new browser.
    chrome::MoveTabsToExistingWindow(source_browser, target_browser, {1});
    // Verify the overlay is visible in the *new* browser holding tab_2.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return IsActorOverlayVisible(target_browser); }));
    // Verify the overlay's child WebView was correctly detached from the source
    // browser window.
    EXPECT_EQ(NumActorOverlayChildren(source_browser), 0u);
    // Verify the WebView was correctly re-attached to the target browser window
    // and is visible.
    ASSERT_EQ(NumActorOverlayChildren(target_browser), 1u);
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return IsActorOverlayChildVisible(target_browser); }));
  }
  // Stop acting on the tab at the end of the test
  state_manager->OnUiEvent(StoppedActingOnTab(tab_2->GetHandle()));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    // Overlay should become invisible in the browser that currently holds tab_1
    return !IsActorOverlayVisible(target_browser);
  }));
  // Verify that stopping actuation only hides the child WebView, but does not
  // destroy it or remove it from the view hierarchy.
  ASSERT_EQ(target_browser->GetBrowserView()
                .GetActiveContentsContainerView()
                ->actor_overlay_view()
                ->children()
                .size(),
            1u);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !target_browser->GetBrowserView()
                .GetActiveContentsContainerView()
                ->actor_overlay_view()
                ->children()[0]
                ->GetVisible();
  }));
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest, RepeatedlyMoveActuatedTabToNewWindow) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  // Initial tab setup: Start with one tab.
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  tabs::TabInterface* tab_1 = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_NE(tab_1, nullptr);
  // Set up callback logic after tab_1 is created.
  TestFuture<void> future;
  ActorUiTabControllerInterface* controller =
      tab_1->GetTabFeatures()->actor_ui_tab_controller();
  controller->SetCallbackForTesting(future.GetCallback());
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(StartingToActOnTab(tab_1->GetHandle(), TaskId(1)),
                           result.GetCallback());
  ExpectOkResult(result);
  // Ensure callback is done.
  ASSERT_TRUE(future.Wait());
  Browser* browser_with_actuated_tab;
  // Loop to repeatedly move the actuated tab to new browser windows. This
  // verifies the overlay's persistence and re-parenting across window changes.
  // The number of iterations (5) is arbitrary and can be adjusted.
  for (int i = 0; i < 5; ++i) {
    // Get the current browser holding the actuated tab.
    browser_with_actuated_tab =
        BrowserWindow::FindBrowserWindowWithWebContents(tab_1->GetContents())
            ->AsBrowserView()
            ->browser();
    ASSERT_NE(browser_with_actuated_tab, nullptr);
    // Verify the overlay is visible in the current browser.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return IsActorOverlayVisible(browser_with_actuated_tab); }));
    // Also verify that the overlay's child WebView exists and is visible.
    ASSERT_EQ(NumActorOverlayChildren(browser_with_actuated_tab), 1u);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return IsActorOverlayChildVisible(browser_with_actuated_tab);
    }));
    // Add a new tab to ensure the source window always has at least two tabs
    // before moving one to a new window (simulates user behavior).
    tabs::TabInterface* new_tab = tabs::TabInterface::GetFromContents(
        &chrome::NewTab(browser_with_actuated_tab));
    ASSERT_NE(new_tab, nullptr);
    ASSERT_EQ(browser_with_actuated_tab->tab_strip_model()->count(), 2);

    // Move the actuated tab (at index 0) to a new browser window.
    TestFuture<void> move_future;
    controller->SetCallbackForTesting(move_future.GetCallback());
    chrome::MoveTabsToNewWindow(browser_with_actuated_tab, {0});
    ASSERT_TRUE(move_future.Wait());
  }
  // After the final move in the loop, update the browser pointer.
  browser_with_actuated_tab =
      BrowserWindow::FindBrowserWindowWithWebContents(tab_1->GetContents())
          ->AsBrowserView()
          ->browser();
  // Stop acting on the tab at the end of the test.
  state_manager->OnUiEvent(StoppedActingOnTab(tab_1->GetHandle()));
  // Overlay should become invisible in the browser that currently holds the
  // actuated tab.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsActorOverlayVisible(browser_with_actuated_tab); }));
  // Verify that stopping actuation only hides the child WebView, but does not
  // destroy it or remove it from the view hierarchy.
  ASSERT_EQ(NumActorOverlayChildren(browser_with_actuated_tab), 1u);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !IsActorOverlayChildVisible(browser_with_actuated_tab);
  }));
}

class ActorOverlayDisabledTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi, {{features::kGlicActorUiOverlayName, "false"}});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorOverlayDisabledTest,
                       PageDoesNotLoadWhenFeatureIsOff) {
  GURL kUrl(chrome::kChromeUIActorOverlayURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(web_contents->GetLastCommittedURL(), kUrl);
  EXPECT_FALSE(web_contents->IsCrashed());
  EXPECT_NE(web_contents->GetTitle(), u"Actor Overlay");
}

// Verifies that the ActorOverlayWindowController should not exist for any
// browser windows since the feature is disabled.
IN_PROC_BROWSER_TEST_F(ActorOverlayDisabledTest,
                       ControllerDoesntExistsForNormalBrowsers) {
  Profile* const profile = browser()->profile();

  // Normal browser window, only the overlay controller's should be null since
  // the feature param for the overlay is disabled, but the GlicActorUi feature
  // is still enabled.
  Browser* const normal_browser = browser();
  ASSERT_EQ(ActorOverlayWindowController::From(normal_browser), nullptr);
  ASSERT_NE(normal_browser->browser_window_features()
                ->tab_strip_model()
                ->GetActiveTab()
                ->GetTabFeatures()
                ->actor_ui_tab_controller(),
            nullptr);

  // Popup window
  Browser* const popup_browser = CreateBrowserForPopup(profile);
  ASSERT_EQ(ActorOverlayWindowController::From(popup_browser), nullptr);
  ASSERT_EQ(popup_browser->browser_window_features()
                ->tab_strip_model()
                ->GetActiveTab()
                ->GetTabFeatures()
                ->actor_ui_tab_controller(),
            nullptr);

  // App window
  Browser* const app_browser = CreateBrowserForApp("test_app_name", profile);
  ASSERT_EQ(ActorOverlayWindowController::From(app_browser), nullptr);
  ASSERT_EQ(app_browser->browser_window_features()
                ->tab_strip_model()
                ->GetActiveTab()
                ->GetTabFeatures()
                ->actor_ui_tab_controller(),
            nullptr);

  // Picture-in-Picture window
  Browser* const pip_browser =
      Browser::Create(Browser::CreateParams::CreateForPictureInPicture(
          "test_app_name", false, profile, false));
  ASSERT_EQ(ActorOverlayWindowController::From(pip_browser), nullptr);
  // Tab Interface is null for Picture-in-Picture windows, so we don't test the
  // tab controller's existence.

  // DevTools window
  Browser* const devtools_browser =
      Browser::Create(Browser::CreateParams::CreateForDevTools(profile));
  ASSERT_EQ(ActorOverlayWindowController::From(devtools_browser), nullptr);
  // Tab Interface is null for DevTools windows, so we don't test the tab
  // controller's existence.
}

}  // namespace
}  // namespace actor::ui
