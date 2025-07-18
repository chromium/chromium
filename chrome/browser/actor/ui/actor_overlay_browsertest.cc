// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_window_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
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

// Verifies that the ActorOverlayWindowController should only exist for normal
// browser windows.
IN_PROC_BROWSER_TEST_F(ActorOverlayTest, ControllerExistsForNormalBrowsers) {
  Profile* const profile = browser()->profile();

  // Normal browser window
  BrowserWindowFeatures* const normal_window_features =
      browser()->browser_window_features();
  ASSERT_TRUE(normal_window_features->actor_overlay_window_controller());

  // Popup window
  BrowserWindowFeatures* const popup_window_features =
      CreateBrowserForPopup(profile)->browser_window_features();
  EXPECT_FALSE(popup_window_features->actor_overlay_window_controller());

  // App window
  BrowserWindowFeatures* const app_window_features =
      CreateBrowserForApp("test_app_name", profile)->browser_window_features();
  EXPECT_FALSE(app_window_features->actor_overlay_window_controller());

  // Picture-in-Picture window
  BrowserWindowFeatures* const pip_window_features =
      Browser::Create(Browser::CreateParams::CreateForPictureInPicture(
                          "test_app_name", false, profile, false))
          ->browser_window_features();
  EXPECT_FALSE(pip_window_features->actor_overlay_window_controller());

  // DevTools window
  BrowserWindowFeatures* const devtools_window_features =
      Browser::Create(Browser::CreateParams::CreateForDevTools(profile))
          ->browser_window_features();
  EXPECT_FALSE(devtools_window_features->actor_overlay_window_controller());
}

IN_PROC_BROWSER_TEST_F(ActorOverlayTest, ViewLifecycleAndVisibility) {
  actor::ui::ActorOverlayWindowController* window_controller =
      browser()->browser_window_features()->actor_overlay_window_controller();
  ASSERT_TRUE(window_controller);

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
  ASSERT_TRUE(overlay_web_view);

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
  ASSERT_TRUE(managed_overlay_web_view);
  EXPECT_FALSE(browser()->GetBrowserView().GetActorOverlayView()->GetVisible());
  EXPECT_EQ(
      browser()->GetBrowserView().GetActorOverlayView()->children().size(), 0u);
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

  // Normal browser window
  BrowserWindowFeatures* const normal_window_features =
      browser()->browser_window_features();
  EXPECT_FALSE(normal_window_features->actor_overlay_window_controller());

  // Popup window
  BrowserWindowFeatures* const popup_window_features =
      CreateBrowserForPopup(profile)->browser_window_features();
  EXPECT_FALSE(popup_window_features->actor_overlay_window_controller());

  // App window
  BrowserWindowFeatures* const app_window_features =
      CreateBrowserForApp("test_app_name", profile)->browser_window_features();
  EXPECT_FALSE(app_window_features->actor_overlay_window_controller());

  // Picture-in-Picture window
  BrowserWindowFeatures* const pip_window_features =
      Browser::Create(Browser::CreateParams::CreateForPictureInPicture(
                          "test_app_name", false, profile, false))
          ->browser_window_features();
  EXPECT_FALSE(pip_window_features->actor_overlay_window_controller());

  // DevTools window
  BrowserWindowFeatures* const devtools_window_features =
      Browser::Create(Browser::CreateParams::CreateForDevTools(profile))
          ->browser_window_features();
  EXPECT_FALSE(devtools_window_features->actor_overlay_window_controller());
}

}  // namespace
