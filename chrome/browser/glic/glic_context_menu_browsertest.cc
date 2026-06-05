// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicContextMenuBrowserTestBase : public GlicBrowserTest {
 protected:
  std::unique_ptr<TestRenderViewContextMenu> CreateContextMenu() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::ContextMenuParams params;
    params.page_url = web_contents->GetVisibleURL();
    auto menu = std::make_unique<TestRenderViewContextMenu>(
        *web_contents->GetPrimaryMainFrame(), params);
    menu->Init();
    return menu;
  }
};

class GlicContextMenuBrowserTest : public GlicContextMenuBrowserTestBase {
 public:
  GlicContextMenuBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicContextMenu}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest, GlicItemPresent) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  auto menu = CreateContextMenu();
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
  EXPECT_TRUE(menu->IsItemEnabled(IDC_CONTENT_CONTEXT_GLIC));
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest, GlicItemPresentForLink) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.link_url = GURL("https://example.com");

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest, GlicInvokeStandard) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  auto menu = CreateContextMenu();

  // Initially no Glic instance.
  EXPECT_EQ(nullptr, GetOnlyGlicInstance());

  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_GLIC, 0);

  // Now Glic should be open.
  ASSERT_OK(WaitForGlicOpen());
  EXPECT_NE(nullptr, GetOnlyGlicInstance());
}

class GlicContextMenuGlicDisabledBrowserTest
    : public GlicContextMenuBrowserTestBase {
 public:
  GlicContextMenuGlicDisabledBrowserTest() {
    feature_list_.InitWithFeatures({features::kGlicContextMenu},
                                   {features::kGlic});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicContextMenuGlicDisabledBrowserTest, GlicItemAbsent) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  auto menu = CreateContextMenu();
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
}

class GlicContextMenuArm2BrowserTest : public GlicContextMenuBrowserTestBase {
 public:
  GlicContextMenuArm2BrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlic, {}},
         {features::kGlicContextMenu,
          {{features::kGlicContextMenuArm.name, "arm2"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicContextMenuArm2BrowserTest, GlicInvokeArm2) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  auto menu = CreateContextMenu();

  // Initially no Glic instance.
  EXPECT_EQ(nullptr, GetOnlyGlicInstance());

  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_GLIC, 0);

  // Now Glic should be open.
  ASSERT_OK(WaitForGlicOpen());
  EXPECT_NE(nullptr, GetOnlyGlicInstance());
}

class GlicContextMenuArm3BrowserTest : public GlicContextMenuBrowserTestBase {
 public:
  GlicContextMenuArm3BrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlic, {}},
         {features::kGlicContextMenu,
          {{features::kGlicContextMenuArm.name, "arm3"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicContextMenuArm3BrowserTest, GlicInvokeArm3) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  auto menu = CreateContextMenu();

  // Initially no Glic instance.
  EXPECT_EQ(nullptr, GetOnlyGlicInstance());

  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_GLIC, 0);

  // Now Glic should be open.
  ASSERT_OK(WaitForGlicOpen());
  EXPECT_NE(nullptr, GetOnlyGlicInstance());
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest, GlicItemAbsentInAppWindow) {
  // Create an app browser window.
  Browser* app_browser = Browser::Create(Browser::CreateParams::CreateForApp(
      "test_app", /*trusted_source=*/false, gfx::Rect(), browser()->profile(),
      /*user_gesture=*/true));

  // Add a tab and navigate to a test page.
  content::WebContents* blank_tab = chrome::AddSelectedTabWithURL(
      app_browser, GetSimpleTestUrl(), ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  ASSERT_TRUE(content::WaitForLoadStop(blank_tab));
  app_browser->window()->Show();

  // Create context menu for the app window.
  content::ContextMenuParams params;
  params.page_url = blank_tab->GetVisibleURL();
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *blank_tab->GetPrimaryMainFrame(), params);
  menu->Init();

  // Verify Glic item is NOT present.
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));

  // Clean up.
  CloseBrowserSynchronously(app_browser);
}

}  // namespace glic
