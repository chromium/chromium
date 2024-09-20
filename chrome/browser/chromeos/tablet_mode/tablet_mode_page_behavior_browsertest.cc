// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chromeos/test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace {

class TabletModePageBehaviorTest : public ChromeOSBrowserUITest {
 public:
  TabletModePageBehaviorTest() = default;

  TabletModePageBehaviorTest(const TabletModePageBehaviorTest&) = delete;
  TabletModePageBehaviorTest& operator=(const TabletModePageBehaviorTest&) =
      delete;

  ~TabletModePageBehaviorTest() override = default;

  // ChromeOSBrowserUITest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDoubleTapToZoomInTabletMode);
    ChromeOSBrowserUITest::SetUp();
  }

  content::WebContents* GetActiveWebContents(Browser* browser) const {
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  blink::web_pref::WebPreferences GetWebKitPreferences(
      content::WebContents* web_contents) const {
    return web_contents->GetOrCreateWebPreferences();
  }

  void ValidateWebPrefs(content::WebContents* web_contents,
                        bool tablet_mode_enabled) const {
    const blink::web_pref::WebPreferences web_prefs =
        GetWebKitPreferences(web_contents);
    if (tablet_mode_enabled) {
      EXPECT_TRUE(web_prefs.double_tap_to_zoom_enabled);
      EXPECT_TRUE(web_prefs.text_autosizing_enabled);
      EXPECT_TRUE(web_prefs.shrinks_viewport_contents_to_fit);
      EXPECT_TRUE(web_prefs.main_frame_resizes_are_orientation_changes);
      EXPECT_FLOAT_EQ(web_prefs.default_minimum_page_scale_factor, 0.25f);
      EXPECT_FLOAT_EQ(web_prefs.default_maximum_page_scale_factor, 5.0f);
    } else {
      EXPECT_FALSE(web_prefs.double_tap_to_zoom_enabled);
      EXPECT_FALSE(web_prefs.text_autosizing_enabled);
      EXPECT_FALSE(web_prefs.shrinks_viewport_contents_to_fit);
      EXPECT_FALSE(web_prefs.main_frame_resizes_are_orientation_changes);
      EXPECT_FLOAT_EQ(web_prefs.default_minimum_page_scale_factor, 1.0f);
      EXPECT_FLOAT_EQ(web_prefs.default_maximum_page_scale_factor, 4.0f);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabletModePageBehaviorTest,
                       TestWebKitPrefsWithTabletModeToggles) {
  EXPECT_FALSE(InTabletMode());
  AddBlankTabAndShow(browser());
  auto* web_contents = GetActiveWebContents(browser());
  ASSERT_TRUE(web_contents);

  // Validate that before tablet mode is enabled, mobile-behavior-related prefs
  // are disabled.
  ValidateWebPrefs(web_contents, false /* tablet_mode_enabled */);

  // Now enable tablet mode, and expect that the same page's web prefs get
  // updated.
  EnterTabletMode();
  ValidateWebPrefs(web_contents, true /* tablet_mode_enabled */);

  // Any newly added pages should have the correct tablet mode prefs.
  Browser* browser_2 = CreateBrowser(browser()->profile());
  auto* web_contents_2 = GetActiveWebContents(browser_2);
  ASSERT_TRUE(web_contents_2);
  ValidateWebPrefs(web_contents_2, true /* tablet_mode_enabled */);

  // Disable tablet mode and expect both pages's prefs are updated.
  ExitTabletMode();
  ValidateWebPrefs(web_contents, false /* tablet_mode_enabled */);
  ValidateWebPrefs(web_contents_2, false /* tablet_mode_enabled */);
}

IN_PROC_BROWSER_TEST_F(TabletModePageBehaviorTest, ExcludeInternalPages) {
  ASSERT_TRUE(AddTabAtIndexToBrowser(
      browser(), 0, GURL(chrome::kChromeUIVersionURL), ui::PAGE_TRANSITION_LINK,
      false /* check_navigation_success */));
  auto* web_contents = GetActiveWebContents(browser());
  ASSERT_TRUE(web_contents);
  EXPECT_STREQ(web_contents->GetLastCommittedURL().spec().c_str(),
               chrome::kChromeUIVersionURL);

  // Now enable tablet mode, and expect that this internal page's web prefs
  // remain unaffected as if tablet mode is off.
  EnterTabletMode();
  ValidateWebPrefs(web_contents, false /* tablet_mode_enabled */);
}

IN_PROC_BROWSER_TEST_F(TabletModePageBehaviorTest, ExcludeHostedApps) {
  browser()->window()->Close();

  // Open a new app window.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(),
      browser()->profile(), true);
  params.initial_show_state = ui::mojom::WindowShowState::kDefault;
  Browser* browser = Browser::Create(params);
  AddBlankTabAndShow(browser);

  ASSERT_TRUE(browser->is_type_app());
  auto* web_contents = GetActiveWebContents(browser);
  ASSERT_TRUE(web_contents);

  // Now enable tablet mode, and expect that the page's web prefs of this hosted
  // app remain unaffected as if tablet mode is off.
  EnterTabletMode();
  ValidateWebPrefs(web_contents, false /* tablet_mode_enabled */);
}

IN_PROC_BROWSER_TEST_F(TabletModePageBehaviorTest, IncludeNTPs) {
  ASSERT_TRUE(AddTabAtIndexToBrowser(
      browser(), 0, GURL(chrome::kChromeUINewTabPageURL),
      ui::PAGE_TRANSITION_LINK, false /* check_navigation_success */));
  auto* web_contents = GetActiveWebContents(browser());
  ASSERT_TRUE(web_contents);
  EXPECT_STREQ(web_contents->GetLastCommittedURL().spec().c_str(),
               chrome::kChromeUINewTabPageURL);

  // Mobile-style Blink prefs should be applied to the NTP in tablet mode.
  EnterTabletMode();
  ValidateWebPrefs(web_contents, true /* tablet_mode_enabled */);
}

}  // namespace
