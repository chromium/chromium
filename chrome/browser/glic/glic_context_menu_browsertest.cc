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
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

namespace glic {

class GlicContextMenuBrowserTestBase : public GlicBrowserTest {
 protected:
  // These tests don't run on Android, so allow browser() use.
  using PlatformBrowserTest::browser;
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
    feature_list_.InitWithFeatures({features::kGlic, features::kGlicContextMenu,
                                    features::kGlicShareImage},
                                   {});
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

IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest, GlicItemAbsentForImage) {
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.has_image_contents = true;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kImage;

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest,
                       GlicPrecedesLensAndReadingModeInPageMenu) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Google Lens option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("http://www.google.com/search?q={searchTerms}");
  data.image_url = "http://www.google.com/searchbyimage/upload";
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  model->SetUserSelectedDefaultSearchProvider(template_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH));
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE));

  auto glic_index = menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC);
  auto lens_index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH);
  auto reading_index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE);

  ASSERT_TRUE(glic_index.has_value());
  ASSERT_TRUE(lens_index.has_value());
  ASSERT_TRUE(reading_index.has_value());

  EXPECT_EQ(glic_index->first, lens_index->first);
  EXPECT_EQ(glic_index->first, reading_index->first);

  EXPECT_LT(glic_index->second, lens_index->second);
  EXPECT_LT(glic_index->second, reading_index->second);
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
  app_browser->GetWindow()->Show();

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

IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest,
                       GlicItemAbsentInPasswordField) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.selection_text = u"selected password";
  params.is_editable = true;
  params.form_control_type = blink::mojom::FormControlType::kInputPassword;

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
}

// Tests that the Glic item precedes the default search provider in the text
// selection context menu under the default feature configuration.
IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest,
                       GlicItemPrecedesSearchProvider) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Search Google option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("http://www.google.com/search?q={searchTerms}");
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  model->SetUserSelectedDefaultSearchProvider(template_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.selection_text = u"test query";
  params.properties[::prefs::kDefaultSearchProviderContextMenuAccessAllowed] =
      "";

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));

  auto glic_index = menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC);
  auto search_index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_SEARCHWEBFOR);

  ASSERT_TRUE(glic_index.has_value());
  ASSERT_TRUE(search_index.has_value());
  EXPECT_EQ(glic_index->first, search_index->first);
  EXPECT_LT(glic_index->second, search_index->second);
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest,
                       GlicItemPrecedesSearchProviderInEditableField) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Search Google option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("http://www.google.com/search?q={searchTerms}");
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  model->SetUserSelectedDefaultSearchProvider(template_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.selection_text = u"test query";
  params.is_editable = true;
  params.properties[::prefs::kDefaultSearchProviderContextMenuAccessAllowed] =
      "";

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));

  auto glic_index = menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC);
  auto search_index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_SEARCHWEBFOR);

  ASSERT_TRUE(glic_index.has_value());
  ASSERT_TRUE(search_index.has_value());
  EXPECT_EQ(glic_index->first, search_index->first);
  EXPECT_LT(glic_index->second, search_index->second);
}

class GlicContextMenuSimplificationBrowserTest
    : public GlicContextMenuBrowserTestBase {
 public:
  GlicContextMenuSimplificationBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicContextMenu, features::kGlicShareImage,
         features::kMenuSimplification},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the Glic item precedes the default search provider in the text
// selection context menu when menu simplification (Desktop Glow Up) is enabled.
// This verifies the AppendRevisedTextSelectionSection() code path.
IN_PROC_BROWSER_TEST_F(GlicContextMenuSimplificationBrowserTest,
                       GlicItemPrecedesSearchProvider) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Search Google option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("http://www.google.com/search?q={searchTerms}");
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  model->SetUserSelectedDefaultSearchProvider(template_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.selection_text = u"test query";
  params.properties[::prefs::kDefaultSearchProviderContextMenuAccessAllowed] =
      "";

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));

  auto glic_index = menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC);
  auto search_index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_SEARCHWEBFOR);

  ASSERT_TRUE(glic_index.has_value());
  ASSERT_TRUE(search_index.has_value());
  EXPECT_EQ(glic_index->first, search_index->first);
  EXPECT_LT(glic_index->second, search_index->second);
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuSimplificationBrowserTest,
                       GlicItemAbsentForLink) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.link_url = GURL("https://example.com");
  params.unfiltered_link_url = GURL("https://example.com");

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
}

class GlicContextMenuStandardBrowserTest
    : public GlicContextMenuBrowserTestBase {
 public:
  GlicContextMenuStandardBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kGlicContextMenu,
                              features::kGlicShareImage},
        /*disabled_features=*/{features::kMenuSimplification,
                               features::kDesktopGlowUp});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the Glic item precedes the default search provider in the text
// selection context menu when menu simplification and Desktop Glow Up are
// explicitly disabled. This verifies the classic InitMenu() code path.
IN_PROC_BROWSER_TEST_F(GlicContextMenuStandardBrowserTest,
                       GlicItemPrecedesSearchProvider) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Search Google option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("http://www.google.com/search?q={searchTerms}");
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  model->SetUserSelectedDefaultSearchProvider(template_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.selection_text = u"test query";
  params.properties[::prefs::kDefaultSearchProviderContextMenuAccessAllowed] =
      "";

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));

  auto glic_index = menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC);
  auto search_index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_SEARCHWEBFOR);

  ASSERT_TRUE(glic_index.has_value());
  ASSERT_TRUE(search_index.has_value());
  EXPECT_EQ(glic_index->first, search_index->first);
  EXPECT_LT(glic_index->second, search_index->second);
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuStandardBrowserTest,
                       GlicItemPrecedesSearchProviderInEditableField) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Search Google option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("http://www.google.com/search?q={searchTerms}");
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  model->SetUserSelectedDefaultSearchProvider(template_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.selection_text = u"test query";
  params.is_editable = true;
  params.properties[::prefs::kDefaultSearchProviderContextMenuAccessAllowed] =
      "";

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_SEARCHWEBFOR));

  auto glic_index = menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC);
  auto search_index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_SEARCHWEBFOR);

  ASSERT_TRUE(glic_index.has_value());
  ASSERT_TRUE(search_index.has_value());
  EXPECT_EQ(glic_index->first, search_index->first);
  EXPECT_LT(glic_index->second, search_index->second);
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuStandardBrowserTest,
                       GlicItemPresentForLink) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.link_url = GURL("https://example.com");
  params.unfiltered_link_url = GURL("https://example.com");

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
}

class GlicTextSelectionContextMenuBrowserTest
    : public GlicContextMenuBrowserTestBase {
 public:
  GlicTextSelectionContextMenuBrowserTest() {
    feature_list_.InitWithFeatures({features::kGlicTextSelectionContextMenu},
                                   {features::kGlicContextMenu});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicTextSelectionContextMenuBrowserTest,
                       GlicItemAbsentForWhitespaceOnlySelection) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.selection_text = u"   \n\t   ";

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
}

IN_PROC_BROWSER_TEST_F(GlicTextSelectionContextMenuBrowserTest,
                       GlicItemPresentForValidSelection) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.selection_text = u"   valid text   ";

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
  auto glic_index = menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC);
  ASSERT_TRUE(glic_index.has_value());
  EXPECT_EQ(glic_index->first->GetLabelAt(glic_index->second),
            l10n_util::GetStringFUTF16(IDS_GLIC_CONTEXT_MENU_ASK_GEMINI_ABOUT,
                                       u"valid text"));
}

}  // namespace glic
