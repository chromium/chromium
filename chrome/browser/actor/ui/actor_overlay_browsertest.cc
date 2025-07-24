// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_overlay_window_controller.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

namespace {

class ActorOverlayTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi, {{features::kGlicActorUiOverlayName, "true"}});
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
  EXPECT_FALSE(web_contents->IsCrashed());
  EXPECT_EQ(web_contents->GetTitle(), u"Actor Overlay");
}

// Verifies that the ActorOverlayWindowController and Actor Ui Tab Controller
// should only exist for normal browser windows.
IN_PROC_BROWSER_TEST_F(ActorOverlayTest, ControllerExistsForNormalBrowsers) {
  Profile* const profile = browser()->profile();

  // Normal browser window
  BrowserWindowFeatures* const normal_window_features =
      browser()->browser_window_features();
  ASSERT_NE(normal_window_features->actor_overlay_window_controller(), nullptr);
  ASSERT_NE(normal_window_features->tab_strip_model()
                ->GetActiveTab()
                ->GetTabFeatures()
                ->actor_ui_tab_controller(),
            nullptr);

  // Popup window
  BrowserWindowFeatures* const popup_window_features =
      CreateBrowserForPopup(profile)->browser_window_features();
  ASSERT_EQ(popup_window_features->actor_overlay_window_controller(), nullptr);
  ASSERT_EQ(popup_window_features->tab_strip_model()
                ->GetActiveTab()
                ->GetTabFeatures()
                ->actor_ui_tab_controller(),
            nullptr);

  // App window
  BrowserWindowFeatures* const app_window_features =
      CreateBrowserForApp("test_app_name", profile)->browser_window_features();
  ASSERT_EQ(app_window_features->actor_overlay_window_controller(), nullptr);
  ASSERT_EQ(app_window_features->tab_strip_model()
                ->GetActiveTab()
                ->GetTabFeatures()
                ->actor_ui_tab_controller(),
            nullptr);

  // Picture-in-Picture window
  BrowserWindowFeatures* const pip_window_features =
      Browser::Create(Browser::CreateParams::CreateForPictureInPicture(
                          "test_app_name", false, profile, false))
          ->browser_window_features();
  ASSERT_EQ(pip_window_features->actor_overlay_window_controller(), nullptr);
  // Tab Interface is null for Picture-in-Picture windows, so we don't test the
  // tab controller's existence.

  // DevTools window
  BrowserWindowFeatures* const devtools_window_features =
      Browser::Create(Browser::CreateParams::CreateForDevTools(profile))
          ->browser_window_features();
  ASSERT_EQ(devtools_window_features->actor_overlay_window_controller(),
            nullptr);
  // Tab Interface is null for DevTools windows, so we don't test the tab
  // controller's existence.
}

// Testing the Actor Overlay Window Controller
IN_PROC_BROWSER_TEST_F(ActorOverlayTest, ViewLifecycleAndVisibility) {
  actor::ui::ActorOverlayWindowController* window_controller =
      browser()->browser_window_features()->actor_overlay_window_controller();
  ASSERT_NE(window_controller, nullptr);

  // The main actor_overlay_view container should initially be hidden. It should
  // also have no children.
  EXPECT_FALSE(browser()->GetBrowserView().GetActorOverlayView()->GetVisible());
  EXPECT_EQ(
      browser()->GetBrowserView().GetActorOverlayView()->children().size(), 0u);
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
  EXPECT_EQ(
      browser()->GetBrowserView().GetActorOverlayView()->children().size(), 1u);
  EXPECT_FALSE(browser()->GetBrowserView().GetActorOverlayView()->GetVisible());

  // Make the added WebView visible, and update the container's visibility.
  overlay_web_view->SetVisible(true);
  window_controller->MaybeUpdateContainerVisibility();

  // Container view should now be visible.
  EXPECT_TRUE(browser()->GetBrowserView().GetActorOverlayView()->GetVisible());
  std::unique_ptr<views::WebView> managed_overlay_web_view =
      window_controller->RemoveChildWebView(overlay_web_view);
  // The raw_ptr to the removed view is now invalid, so set it to nullptr.
  overlay_web_view = nullptr;

  // Confirm managed WebView is not null and the container should become hidden
  // again
  ASSERT_NE(managed_overlay_web_view, nullptr);
  EXPECT_FALSE(browser()->GetBrowserView().GetActorOverlayView()->GetVisible());
  EXPECT_EQ(
      browser()->GetBrowserView().GetActorOverlayView()->children().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest, SendStartEventAndStopEvent) {
  Profile* const profile = browser()->profile();
  actor::ui::ActorUiStateManagerInterface* state_manager =
      actor::ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  // actor::PageTarget page_target(gfx::Point(100, 200));
  tabs::TabHandle tab_handle =
      browser()->tab_strip_model()->GetActiveTab()->GetHandle();
  state_manager->OnUiEvent(
      actor::ui::StartingToActOnTab(tab_handle, actor::TaskId(1)),
      base::DoNothing());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetBrowserView().GetActorOverlayView()->GetVisible();
  }));
  EXPECT_EQ(
      browser()->GetBrowserView().GetActorOverlayView()->children().size(), 1u);
  EXPECT_TRUE(browser()
                  ->GetBrowserView()
                  .GetActorOverlayView()
                  ->children()[0]
                  ->GetVisible());
  state_manager->OnUiEvent(actor::ui::StoppedActingOnTab(tab_handle));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !browser()->GetBrowserView().GetActorOverlayView()->GetVisible();
  }));
  EXPECT_EQ(
      browser()->GetBrowserView().GetActorOverlayView()->children().size(), 1u);
  EXPECT_FALSE(browser()
                   ->GetBrowserView()
                   .GetActorOverlayView()
                   ->children()[0]
                   ->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest, OverlayHidesOnTabBackgrounding) {
  Profile* const profile = browser()->profile();
  actor::ui::ActorUiStateManagerInterface* state_manager =
      actor::ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabHandle tab_handle =
      browser()->tab_strip_model()->GetActiveTab()->GetHandle();
  state_manager->OnUiEvent(
      actor::ui::StartingToActOnTab(tab_handle, actor::TaskId(1)),
      base::DoNothing());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetBrowserView().GetActorOverlayView()->GetVisible();
  }));
  EXPECT_EQ(
      browser()->GetBrowserView().GetActorOverlayView()->children().size(), 1u);
  EXPECT_TRUE(browser()
                  ->GetBrowserView()
                  .GetActorOverlayView()
                  ->children()[0]
                  ->GetVisible());
  browser()->tab_strip_model()->AppendWebContents(
      content::WebContents::Create(content::WebContents::CreateParams(profile)),
      /*foreground=*/true);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !browser()->GetBrowserView().GetActorOverlayView()->GetVisible();
  }));
  EXPECT_EQ(
      browser()->GetBrowserView().GetActorOverlayView()->children().size(), 1u);
  EXPECT_FALSE(browser()
                   ->GetBrowserView()
                   .GetActorOverlayView()
                   ->children()[0]
                   ->GetVisible());
  browser()->tab_strip_model()->ActivateTabAt(0);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetBrowserView().GetActorOverlayView()->GetVisible();
  }));
  EXPECT_EQ(
      browser()->GetBrowserView().GetActorOverlayView()->children().size(), 1u);
  EXPECT_TRUE(browser()
                  ->GetBrowserView()
                  .GetActorOverlayView()
                  ->children()[0]
                  ->GetVisible());
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
  BrowserWindowFeatures* const normal_window_features =
      browser()->browser_window_features();
  ASSERT_EQ(normal_window_features->actor_overlay_window_controller(), nullptr);
  ASSERT_NE(normal_window_features->tab_strip_model()
                ->GetActiveTab()
                ->GetTabFeatures()
                ->actor_ui_tab_controller(),
            nullptr);

  // Popup window
  BrowserWindowFeatures* const popup_window_features =
      CreateBrowserForPopup(profile)->browser_window_features();
  ASSERT_EQ(popup_window_features->actor_overlay_window_controller(), nullptr);
  ASSERT_EQ(popup_window_features->tab_strip_model()
                ->GetActiveTab()
                ->GetTabFeatures()
                ->actor_ui_tab_controller(),
            nullptr);

  // App window
  BrowserWindowFeatures* const app_window_features =
      CreateBrowserForApp("test_app_name", profile)->browser_window_features();
  ASSERT_EQ(app_window_features->actor_overlay_window_controller(), nullptr);
  ASSERT_EQ(app_window_features->tab_strip_model()
                ->GetActiveTab()
                ->GetTabFeatures()
                ->actor_ui_tab_controller(),
            nullptr);

  // Picture-in-Picture window
  BrowserWindowFeatures* const pip_window_features =
      Browser::Create(Browser::CreateParams::CreateForPictureInPicture(
                          "test_app_name", false, profile, false))
          ->browser_window_features();
  ASSERT_EQ(pip_window_features->actor_overlay_window_controller(), nullptr);
  // Tab Interface is null for Picture-in-Picture windows, so we don't test the
  // tab controller's existence.

  // DevTools window
  BrowserWindowFeatures* const devtools_window_features =
      Browser::Create(Browser::CreateParams::CreateForDevTools(profile))
          ->browser_window_features();
  ASSERT_EQ(devtools_window_features->actor_overlay_window_controller(),
            nullptr);
  // Tab Interface is null for DevTools windows, so we don't test the tab
  // controller's existence.
}

}  // namespace
