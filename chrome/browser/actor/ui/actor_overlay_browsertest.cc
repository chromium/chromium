// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/ui/actor_overlay_ui.h"
#include "chrome/browser/actor/ui/actor_overlay_web_view.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/actor_ui_window_controller.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/test/split_view_browser_test_mixin.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/display/display_switches.h"

namespace actor::ui {
namespace {
using actor::mojom::ActionResultPtr;
using base::test::TestFuture;

ActorOverlayWebView* GetActorOverlayWebView(Browser* browser) {
  return browser->GetBrowserView()
      .GetActiveContentsContainerView()
      ->actor_overlay_web_view();
}

bool IsActorOverlayVisible(Browser* browser) {
  return GetActorOverlayWebView(browser)->GetVisible();
}

content::WebContents* GetActorOverlayWebViewWebContents(Browser* browser) {
  return GetActorOverlayWebView(browser)->web_contents();
}

class ActorOverlayTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlicActorUi,
                               {{features::kGlicActorUiOverlayName, "true"}}},
                              {features::kGlicActorUiMagicCursor, {}}},
        /*disabled_features=*/{});
    InProcessBrowserTest::SetUp();
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
  EXPECT_TRUE(ActorOverlayUI::IsActorOverlayWebContents(web_contents));
  EXPECT_FALSE(web_contents->IsCrashed());
  EXPECT_EQ(web_contents->GetTitle(), u"Actor Overlay");
  // Check WebContents from another WebUIController.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUISettingsURL)));
  EXPECT_FALSE(ActorOverlayUI::IsActorOverlayWebContents(web_contents));
  // Check WebContents from a non WebUIController.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  EXPECT_FALSE(ActorOverlayUI::IsActorOverlayWebContents(web_contents));
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest, PageDoesNotLoadInOTRBrowser) {
  GURL kUrl(chrome::kChromeUIActorOverlayURL);
  Browser* otr_browser = OpenURLOffTheRecord(browser()->profile(), kUrl);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(otr_browser, kUrl));
  content::WebContents* web_contents =
      otr_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_NE(web_contents->GetTitle(), u"Actor Overlay");
  EXPECT_FALSE(ActorOverlayUI::IsActorOverlayWebContents(web_contents));
}

// Verifies that the ActorUiWindowController and Actor Ui Tab Controller
// should only exist for normal browser windows.
IN_PROC_BROWSER_TEST_F(ActorOverlayTest, ControllerExistsForNormalBrowsers) {
  Profile* const profile = browser()->profile();

  // Normal browser window
  Browser* const normal_browser = browser();
  ASSERT_NE(ActorUiWindowController::From(normal_browser), nullptr);
  ASSERT_NE(ActorUiTabController::From(normal_browser->browser_window_features()
                                           ->tab_strip_model()
                                           ->GetActiveTab()),
            nullptr);

  // Popup window
  Browser* const popup_browser = CreateBrowserForPopup(profile);
  ASSERT_EQ(ActorUiWindowController::From(popup_browser), nullptr);
  ASSERT_EQ(ActorUiTabController::From(popup_browser->browser_window_features()
                                           ->tab_strip_model()
                                           ->GetActiveTab()),
            nullptr);

  // App window
  Browser* const app_browser = CreateBrowserForApp("test_app_name", profile);
  ASSERT_EQ(ActorUiWindowController::From(app_browser), nullptr);
  ASSERT_EQ(ActorUiTabController::From(app_browser->browser_window_features()
                                           ->tab_strip_model()
                                           ->GetActiveTab()),
            nullptr);

  // Picture-in-Picture window
  Browser* const pip_browser =
      Browser::Create(Browser::CreateParams::CreateForPictureInPicture(
          "test_app_name", false, profile, false));
  ASSERT_EQ(ActorUiWindowController::From(pip_browser), nullptr);
  // Tab Interface is null for Picture-in-Picture windows, so we don't test the
  // tab controller's existence.

  // DevTools window
  Browser* const devtools_browser =
      Browser::Create(Browser::CreateParams::CreateForDevTools(profile));
  ASSERT_EQ(ActorUiWindowController::From(devtools_browser), nullptr);
  // Tab Interface is null for DevTools windows, so we don't test the tab
  // controller's existence.
}

// Testing the Actor Overlay Window Controller
IN_PROC_BROWSER_TEST_F(ActorOverlayTest, WebViewLifecycleAndVisibility) {
  ActorUiWindowController* window_controller =
      ActorUiWindowController::From(browser());
  ASSERT_NE(window_controller, nullptr);
  ActorUiContentsContainerController* contents_controller =
      window_controller->GetControllerForWebContents(
          browser()->GetActiveTabInterface()->GetContents());
  ASSERT_NE(contents_controller, nullptr);

  // The actor overlay web webview should initially be hidden.
  EXPECT_FALSE(IsActorOverlayVisible(browser()));

  // Verify web contents have not been attached yet.
  EXPECT_EQ(GetActorOverlayWebViewWebContents(browser()), nullptr);

  // Make the scrim visible.
  TestFuture<void> future1;
  contents_controller->OnOverlayStateChanged(
      /*is_visible=*/true, ActorOverlayState(), future1.GetCallback());
  EXPECT_TRUE(future1.Wait());
  // Actor Overlay WebView should now be visible.
  EXPECT_TRUE(IsActorOverlayVisible(browser()));

  TestFuture<void> future2;
  contents_controller->OnOverlayStateChanged(
      /*is_visible=*/false, ActorOverlayState(), future2.GetCallback());
  EXPECT_TRUE(future2.Wait());
  // Confirm Actor Overlay WebView is hidden.
  EXPECT_FALSE(IsActorOverlayVisible(browser()));
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest, SendStartEventAndStopEvent) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabHandle tab_handle = browser()->GetActiveTabInterface()->GetHandle();
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(StartingToActOnTab(tab_handle, TaskId(1)),
                           result.GetCallback());
  ExpectOkResult(result);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser()); }));
  EXPECT_NE(GetActorOverlayWebViewWebContents(browser()), nullptr);
  state_manager->OnUiEvent(StoppedActingOnTab(tab_handle));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsActorOverlayVisible(browser()); }));
  // The web contents for the actor overlay are not cleaned up until the web
  // view is destroyed, so they should still be attached even when we stop
  // acting on the tab.
  EXPECT_NE(GetActorOverlayWebViewWebContents(browser()), nullptr);
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest, OverlayHidesOnTabBackgrounding) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabHandle tab_handle = browser()->GetActiveTabInterface()->GetHandle();
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(StartingToActOnTab(tab_handle, TaskId(1)),
                           result.GetCallback());
  ExpectOkResult(result);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser()); }));
  EXPECT_NE(GetActorOverlayWebViewWebContents(browser()), nullptr);
  browser()->tab_strip_model()->AppendWebContents(
      content::WebContents::Create(content::WebContents::CreateParams(profile)),
      /*foreground=*/true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsActorOverlayVisible(browser()); }));
  // After switching to a new, non-actuated tab, the overlay is hidden. The
  // webview instance is persistent within the ActiveContentsContainerView.
  // Switching tabs only hides the overlay. It's web contents are still attached
  // at this point and are only cleaned up when the webview itself is destroyed.
  EXPECT_NE(GetActorOverlayWebViewWebContents(browser()), nullptr);
  browser()->tab_strip_model()->ActivateTabAt(0);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser()); }));
  EXPECT_NE(GetActorOverlayWebViewWebContents(browser()), nullptr);
}

// TODO(crbug.com/452105133): Disabled on Linux dbg due to flakiness.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_RepeatedlyMoveTabBetweenWindows \
  DISABLED_RepeatedlyMoveTabBetweenWindows
#else
#define MAYBE_RepeatedlyMoveTabBetweenWindows RepeatedlyMoveTabBetweenWindows
#endif
IN_PROC_BROWSER_TEST_F(ActorOverlayTest,
                       MAYBE_RepeatedlyMoveTabBetweenWindows) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  // Initial tab setup: Create 3 tabs in the starting browser window.
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  tabs::TabInterface* tab_1 = browser()->GetActiveTabInterface();
  ASSERT_NE(tab_1, nullptr);
  tabs::TabInterface* tab_2 =
      tabs::TabInterface::GetFromContents(&chrome::NewTab(browser()));
  ASSERT_NE(tab_2, nullptr);

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
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser_1); }));
  EXPECT_NE(GetActorOverlayWebViewWebContents(browser_1), nullptr);
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
    // The web contents should also be attached to the webview in the target
    // browser.
    EXPECT_NE(GetActorOverlayWebViewWebContents(target_browser), nullptr);
    // The actuated tab has left the source browser, so the overlay is hidden.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !IsActorOverlayVisible(source_browser); }));
    // The webview instance is persistent to its browser window. Moving the tab
    // to a different browser only hides the overlay; its web contents remain
    // attached. Once the web contents has been attached to the webview for a
    // browser window, it will only be cleaned up when the webview itself is
    // destroyed.
    EXPECT_NE(GetActorOverlayWebViewWebContents(source_browser), nullptr);
  }
  // Stop acting on the tab at the end of the test
  state_manager->OnUiEvent(StoppedActingOnTab(tab_2->GetHandle()));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    // Overlay should become invisible in the browser that currently holds tab_1
    return !IsActorOverlayVisible(target_browser);
  }));
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest, RepeatedlyMoveActuatedTabToNewWindow) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  // Initial tab setup: Start with one tab.
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  tabs::TabInterface* tab_1 = browser()->GetActiveTabInterface();
  ASSERT_NE(tab_1, nullptr);
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(StartingToActOnTab(tab_1->GetHandle(), TaskId(1)),
                           result.GetCallback());
  ExpectOkResult(result);
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
    // Verify the overlay's web contents were correctly attached.
    EXPECT_NE(GetActorOverlayWebViewWebContents(browser_with_actuated_tab),
              nullptr);
    // Add a new tab to ensure the source window always has at least two tabs
    // before moving one to a new window (simulates user behavior).
    tabs::TabInterface* new_tab = tabs::TabInterface::GetFromContents(
        &chrome::NewTab(browser_with_actuated_tab));
    ASSERT_NE(new_tab, nullptr);
    ASSERT_EQ(browser_with_actuated_tab->tab_strip_model()->count(), 2);

    // Move the actuated tab (at index 0) to a new browser window.
    chrome::MoveTabsToNewWindow(browser_with_actuated_tab, {0});
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
  EXPECT_EQ(GetActorOverlayWebViewWebContents(browser_with_actuated_tab),
            nullptr);
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest,
                       InputAndA11yInputEventsIgnoredWhenOverlayVisible) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabHandle tab_handle = browser()->GetActiveTabInterface()->GetHandle();

  // Check initial state: Input and A11y Input should NOT be ignored by default.
  EXPECT_FALSE(browser()
                   ->GetActiveTabInterface()
                   ->GetContents()
                   ->ShouldIgnoreInputEventsForTesting());
  EXPECT_FALSE(browser()
                   ->GetActiveTabInterface()
                   ->GetContents()
                   ->ShouldIgnoreA11yInputEventsForTesting());

  // Start actuating on the tab.
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(StartingToActOnTab(tab_handle, TaskId(1)),
                           result.GetCallback());
  ExpectOkResult(result);

  // Wait for the overlay to become visible.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser()); }));

  // Check that input and a11y input should be ignored.
  EXPECT_TRUE(browser()
                  ->GetActiveTabInterface()
                  ->GetContents()
                  ->ShouldIgnoreInputEventsForTesting());
  EXPECT_TRUE(browser()
                  ->GetActiveTabInterface()
                  ->GetContents()
                  ->ShouldIgnoreA11yInputEventsForTesting());

  // Add a new tab, which is the new active tab
  tabs::TabInterface* tab_2 =
      tabs::TabInterface::GetFromContents(&chrome::NewTab(browser()));
  ASSERT_NE(tab_2, nullptr);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  // Wait for overlay to become invisible for the newly added tab.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsActorOverlayVisible(browser()); }));
  // Check that input and a11y input is NOT ignored for newly added tab.
  EXPECT_FALSE(browser()
                   ->GetActiveTabInterface()
                   ->GetContents()
                   ->ShouldIgnoreInputEventsForTesting());
  EXPECT_FALSE(browser()
                   ->GetActiveTabInterface()
                   ->GetContents()
                   ->ShouldIgnoreA11yInputEventsForTesting());
  // Activate the actuating tab
  browser()->tab_strip_model()->ActivateTabAt(0);
  // Wait for overlay to become visible on actuating tab.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser()); }));
  // Check that input and a11y input is ignored for actuating tab.
  EXPECT_TRUE(browser()
                  ->GetActiveTabInterface()
                  ->GetContents()
                  ->ShouldIgnoreInputEventsForTesting());
  EXPECT_TRUE(browser()
                  ->GetActiveTabInterface()
                  ->GetContents()
                  ->ShouldIgnoreA11yInputEventsForTesting());

  // Stop actuating on the tab.
  state_manager->OnUiEvent(StoppedActingOnTab(tab_handle));

  // Wait for the overlay to become invisible.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsActorOverlayVisible(browser()); }));

  // Check that input and a11y input is NOT ignored for previously actuating
  // tab.
  EXPECT_FALSE(browser()
                   ->GetActiveTabInterface()
                   ->GetContents()
                   ->ShouldIgnoreInputEventsForTesting());
  EXPECT_FALSE(browser()
                   ->GetActiveTabInterface()
                   ->GetContents()
                   ->ShouldIgnoreA11yInputEventsForTesting());
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest, OverlayIsIgnoredByAccessibility) {
  views::WebView* overlay_web_view = browser()
                                         ->GetBrowserView()
                                         .GetActiveContentsContainerView()
                                         ->actor_overlay_web_view();
  ASSERT_NE(overlay_web_view, nullptr);
  EXPECT_EQ(overlay_web_view->GetFocusBehavior(),
            views::View::FocusBehavior::NEVER);
  EXPECT_TRUE(overlay_web_view->GetViewAccessibility().GetIsIgnored());
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest,
                       FindInPageDisabledWhenOverlayVisibleMultiTab) {
  // Verify that the default state is enabled, otherwise we may see a false
  // positive in this test.
  ASSERT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));
  // Add a second tab to the browser.
  chrome::NewTab(browser());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  // Activate the first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  // Start actuating on the first tab and wait for the overlay to be visible.
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabHandle tab_handle = browser()->GetActiveTabInterface()->GetHandle();
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(StartingToActOnTab(tab_handle, TaskId(1)),
                           result.GetCallback());
  ExpectOkResult(result);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser()); }));
  // Verify that the FIP command is disabled because the actor overlay is
  // visible.
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FIND));
  // Activate the second tab and verify the overlay is not visible.
  browser()->tab_strip_model()->ActivateTabAt(1);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsActorOverlayVisible(browser()); }));
  // Verify that the FIP command is enabled on the non-actuating tab.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));
  // Activate the first tab again and verify the overlay is visible.
  browser()->tab_strip_model()->ActivateTabAt(0);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser()); }));
  // Verify that the FIP command is still disabled.
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FIND));
  // Stop actuating.
  state_manager->OnUiEvent(StoppedActingOnTab(tab_handle));
  // Wait for actor overlay to be hidden.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsActorOverlayVisible(browser()); }));
  // Verify that the FIP command is back to enabled.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest,
                       FindInPageDisabledWhenOverlayVisibleMultiWindow) {
  Browser* browser1 = browser();
  // Verify that the default state is enabled for first browser.
  ASSERT_TRUE(chrome::IsCommandEnabled(browser1, IDC_FIND));
  // Create a second browser window.
  Profile* const profile = browser1->profile();
  Browser* browser2 = CreateBrowser(profile);
  ASSERT_NE(browser2, browser1);
  // Verify that the default state is enabled for the second browser.
  ASSERT_TRUE(chrome::IsCommandEnabled(browser2, IDC_FIND));
  // Activate the first browser window.
  browser1->window()->Activate();
  // Start actuating on the first browser window and wait for overlay to
  // visible.
  ActorUiStateManagerInterface* state_manager =
      ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabHandle tab_handle = browser1->GetActiveTabInterface()->GetHandle();
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(StartingToActOnTab(tab_handle, TaskId(1)),
                           result.GetCallback());
  ExpectOkResult(result);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser1); }));
  // Verify that the FIP command is disabled in the first browser.
  EXPECT_FALSE(chrome::IsCommandEnabled(browser1, IDC_FIND));
  // Activate the second browser window.
  browser2->window()->Activate();
  // Verify that the FIP command is enabled in the second browser.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser2, IDC_FIND));
  // Activate the first browser window again.
  browser1->window()->Activate();
  // Verify that the FIP command is still disabled in the first browser.
  EXPECT_FALSE(chrome::IsCommandEnabled(browser1, IDC_FIND));
  // Stop actuating and wait for overlay to be hidden.
  state_manager->OnUiEvent(StoppedActingOnTab(tab_handle));
  // Wait for actor overlay to be hidden.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !IsActorOverlayVisible(browser1); }));
  // Verify that the FIP command is enabled in both browsers.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser1, IDC_FIND));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser2, IDC_FIND));
}

class ActorOverlaySplitViewTest
    : public SplitViewBrowserTestMixin<InProcessBrowserTest> {
 public:
  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {
        {features::kGlicActorUi, {{features::kGlicActorUiOverlayName, "true"}}},
        {features::kGlicActorUiMagicCursor, {}},
        {features::kSideBySide, {}}};
  }

  void CreateSplitView() {
    auto* tab_strip_model = browser()->tab_strip_model();
    const int active_index = tab_strip_model->active_index();

    RunScheduledLayouts();
    chrome::NewSplitTab(browser(),
                        split_tabs::SplitTabCreatedSource::kToolbarButton);
    EXPECT_TRUE(content::WaitForLoadStop(
        tab_strip_model->GetWebContentsAt(active_index + 1)));
    RunScheduledLayouts();
  }
};

IN_PROC_BROWSER_TEST_F(ActorOverlaySplitViewTest,
                       FindInPageDisabledWhenOverlayVisibleAndActiveTab) {
  CreateSplitView();
  // Activate the right split view tab.
  browser()->tab_strip_model()->ActivateTabAt(1);
  ASSERT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));
  // Start actuation on right split view tab and wait for overlay to be visible.
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabHandle tab_handle = browser()->GetActiveTabInterface()->GetHandle();
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(StartingToActOnTab(tab_handle, TaskId(1)),
                           result.GetCallback());
  ExpectOkResult(result);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsActorOverlayVisible(browser()); }));
  // Verify FIP is disabled.
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FIND));
  // Activate left split view tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  // Verify FIP is enabled.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));
  // Activate right split view tab.
  browser()->tab_strip_model()->ActivateTabAt(1);
  // Verify FIP is still disabled.
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FIND));
  // Stop actuating and wait for overlay to be hidden.
  state_manager->OnUiEvent(StoppedActingOnTab(tab_handle));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsActorOverlayVisible(browser()); }));
  // Verify FIP is enabled when either split view tabs are activated.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));
}

std::string WaitForCursorTransformScript() {
  return R"(
    (async () => {
      return new Promise(resolve => {
        const check = () => {
          const val = document.querySelector('actor-overlay-app')
                              ?.shadowRoot
                              ?.querySelector('#magicCursor')
                              ?.style?.transform;
          if (val && val.startsWith('translate')) {
             resolve(val);
          } else {
             requestAnimationFrame(check);
          }
        };
        check();
      });
    })();
  )";
}

class ActorOverlayMagicCursorDeviceScaleFactorTest
    : public ActorOverlayTest,
      public testing::WithParamInterface<float> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ActorOverlayTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::NumberToString(GetParam()));
  }
};

IN_PROC_BROWSER_TEST_P(ActorOverlayMagicCursorDeviceScaleFactorTest,
                       MagicCursorMovesToCoordinates) {
  ActorUiWindowController* window_controller =
      ActorUiWindowController::From(browser());
  ASSERT_NE(window_controller, nullptr);

  ActorUiContentsContainerController* contents_controller =
      window_controller->GetControllerForWebContents(
          browser()->GetActiveTabInterface()->GetContents());
  ASSERT_NE(contents_controller, nullptr);

  // Initialize UI
  ActorOverlayState init_state;
  init_state.is_active = true;

  TestFuture<void> init_future;
  contents_controller->OnOverlayStateChanged(/*is_visible=*/true, init_state,
                                             init_future.GetCallback());
  ASSERT_TRUE(init_future.Wait());

  content::WebContents* overlay_contents =
      GetActorOverlayWebViewWebContents(browser());
  ASSERT_NE(overlay_contents, nullptr);

  ASSERT_TRUE(content::WaitForLoadStop(overlay_contents));

  // Move Cursor
  gfx::Point physical_target(75, 150);
  ActorOverlayState move_state;
  move_state.is_active = true;
  move_state.mouse_target = physical_target;

  TestFuture<void> move_future;
  contents_controller->OnOverlayStateChanged(/*is_visible=*/true, move_state,
                                             move_future.GetCallback());
  ASSERT_TRUE(move_future.Wait());

  // Verify Coordinates
  float scale_factor =
      overlay_contents->GetRenderWidgetHostView()->GetDeviceScaleFactor();
  EXPECT_EQ(scale_factor, GetParam());
  double expected_x = physical_target.x() / scale_factor;
  double expected_y = physical_target.y() / scale_factor;

  std::string expected_transform = "translate(" +
                                   base::NumberToString(expected_x) + "px, " +
                                   base::NumberToString(expected_y) + "px)";
  std::string actual_transform =
      content::EvalJs(overlay_contents, WaitForCursorTransformScript())
          .ExtractString();

  LOG(INFO) << "Expected: " << expected_transform;
  LOG(INFO) << "Actual: " << actual_transform;
  EXPECT_EQ(actual_transform, expected_transform);
}

// Run with 0.5 (Low DPI), 1.0 (Standard), and 1.5 (High DPI).
INSTANTIATE_TEST_SUITE_P(All,
                         ActorOverlayMagicCursorDeviceScaleFactorTest,
                         testing::Values(0.5f, 1.0f, 1.5f));

std::string WaitForCursorClickingScript() {
  return R"(
    (async () => {
      return new Promise(resolve => {
        const check = () => {
          const cursor = document.querySelector('actor-overlay-app')
                              ?.shadowRoot
                              ?.querySelector('#magicCursor');
          if (cursor?.classList.contains('clicking')) {
             resolve(true);
          } else {
             requestAnimationFrame(check);
          }
        };
        check();
      });
    })();
  )";
}

class ActorOverlayMagicCursorTest : public ActorOverlayTest {};

IN_PROC_BROWSER_TEST_F(ActorOverlayMagicCursorTest,
                       MagicCursorTriggersClickAnimation) {
  ActorUiWindowController* window_controller =
      ActorUiWindowController::From(browser());
  ActorUiContentsContainerController* contents_controller =
      window_controller->GetControllerForWebContents(
          browser()->GetActiveTabInterface()->GetContents());

  // Initialize overlay
  ActorOverlayState init_state;
  init_state.is_active = true;
  TestFuture<void> init_future;
  contents_controller->OnOverlayStateChanged(true, init_state,
                                             init_future.GetCallback());
  ASSERT_TRUE(init_future.Wait());

  // Move Cursor
  ActorOverlayState move_state;
  move_state.is_active = true;
  move_state.mouse_target = gfx::Point(100, 100);
  TestFuture<void> move_future;
  contents_controller->OnOverlayStateChanged(true, move_state,
                                             move_future.GetCallback());
  ASSERT_TRUE(move_future.Wait());

  content::WebContents* overlay_contents =
      GetActorOverlayWebViewWebContents(browser());
  ASSERT_TRUE(content::WaitForLoadStop(overlay_contents));

  // Trigger click animation
  ActorOverlayState click_state;
  click_state.is_active = true;
  click_state.mouse_down = true;
  TestFuture<void> click_future;
  contents_controller->OnOverlayStateChanged(true, click_state,
                                             click_future.GetCallback());

  // Verify clicking animation started
  EXPECT_EQ(content::EvalJs(overlay_contents, WaitForCursorClickingScript()),
            true);
  ASSERT_TRUE(click_future.Wait());

  // Verify clicking animation stopped
  bool has_class =
      content::EvalJs(
          overlay_contents,
          "document.querySelector('actor-overlay-app').shadowRoot"
          ".querySelector('#magicCursor').classList.contains('clicking')")
          .ExtractBool();
  EXPECT_FALSE(has_class);
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

IN_PROC_BROWSER_TEST_F(ActorOverlayDisabledTest,
                       OnOverlayStateChangedRunsCallbackWhenOverlayIsNull) {
  ActorUiWindowController* window_controller =
      ActorUiWindowController::From(browser());
  // The window controller should still exist when the GlicActorUi feature is
  // on, even if the overlay feature is disabled.
  ASSERT_NE(window_controller, nullptr);

  ActorUiContentsContainerController* contents_controller =
      window_controller->GetControllerForWebContents(
          browser()->GetActiveTabInterface()->GetContents());
  ASSERT_NE(contents_controller, nullptr);

  // In this test setup, the ActorOverlayWebView member of contents_controller
  // is null because the GlicActorUiOverlay feature is disabled. This verifies
  // that the callback passed in is still run when the webview is null.
  base::test::TestFuture<void> future;
  contents_controller->OnOverlayStateChanged(
      /*is_visible=*/true, ActorOverlayState(), future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

class GlicActorDisabledTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitAndDisableFeature(features::kGlicActorUi);
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that the ActorUiWindowController should not exist for any
// browser windows since the feature is disabled.
IN_PROC_BROWSER_TEST_F(GlicActorDisabledTest,
                       ControllerDoesntExistsForNormalBrowsers) {
  Profile* const profile = browser()->profile();

  // Normal browser window
  Browser* const normal_browser = browser();
  ASSERT_EQ(ActorUiWindowController::From(normal_browser), nullptr);
  ASSERT_EQ(ActorUiTabController::From(normal_browser->browser_window_features()
                                           ->tab_strip_model()
                                           ->GetActiveTab()),
            nullptr);

  // Popup window
  Browser* const popup_browser = CreateBrowserForPopup(profile);
  ASSERT_EQ(ActorUiWindowController::From(popup_browser), nullptr);
  ASSERT_EQ(ActorUiTabController::From(popup_browser->browser_window_features()
                                           ->tab_strip_model()
                                           ->GetActiveTab()),
            nullptr);

  // App window
  Browser* const app_browser = CreateBrowserForApp("test_app_name", profile);
  ASSERT_EQ(ActorUiWindowController::From(app_browser), nullptr);
  ASSERT_EQ(ActorUiTabController::From(app_browser->browser_window_features()
                                           ->tab_strip_model()
                                           ->GetActiveTab()),
            nullptr);

  // Picture-in-Picture window
  Browser* const pip_browser =
      Browser::Create(Browser::CreateParams::CreateForPictureInPicture(
          "test_app_name", false, profile, false));
  ASSERT_EQ(ActorUiWindowController::From(pip_browser), nullptr);
  // Tab Interface is null for Picture-in-Picture windows, so we don't test the
  // tab controller's existence.

  // DevTools window
  Browser* const devtools_browser =
      Browser::Create(Browser::CreateParams::CreateForDevTools(profile));
  ASSERT_EQ(ActorUiWindowController::From(devtools_browser), nullptr);
  // Tab Interface is null for DevTools windows, so we don't test the tab
  // controller's existence.
}

}  // namespace
}  // namespace actor::ui
