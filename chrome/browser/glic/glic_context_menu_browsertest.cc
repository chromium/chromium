// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/glic/glic_selection_observer.h"
#include "chrome/browser/glic/host/glic_no_webview_contents_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "printing/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/test_print_view_manager_for_request_preview.h"
#endif

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
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass;
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
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest, GlicItemAbsentForLink) {
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass;
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

IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest,
                       GlicItemPresentForLinkWithTextSelection) {
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.link_url = GURL("https://example.com");
  params.unfiltered_link_url = GURL("https://example.com");
  params.selection_text = u"selected text";

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
}

class GlicContextMenuShareImageDisabledBrowserTest
    : public GlicContextMenuBrowserTestBase {
 public:
  GlicContextMenuShareImageDisabledBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicContextMenu},
        {features::kGlicShareImage});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicContextMenuShareImageDisabledBrowserTest,
                       GlicItemAbsentForImage) {
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass;
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
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLICSHAREIMAGE));
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest,
                       GlicItemPresentForTextSelection) {
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.selection_text = u"selected text";

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest,
                       GlicItemPresentForImageInTextSelection) {
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.selection_text = u"selected text";
  params.has_image_contents = true;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kImage;

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLICSHAREIMAGE));
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuBrowserTest,
                       GlicPrecedesLensAndReadingModeInPageMenu) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Google Lens option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("https://www.google.com/search?q={searchTerms}");
  data.image_url = "https://www.google.com/searchbyimage/upload";
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
  BrowserWindowInterface* app_browser =
      CreateBrowserWindow(BrowserWindowCreateParams::CreateForApp(
          "test_app", /*trusted_source=*/false, gfx::Rect(),
          browser()->GetProfile(),
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
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Search Google option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("https://www.google.com/search?q={searchTerms}");
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
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Search Google option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("https://www.google.com/search?q={searchTerms}");
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
        /*enabled_features=*/{features::kGlic, features::kGlicContextMenu,
                              features::kGlicShareImage,
                              features::kMenuSimplification},
        /*disabled_features=*/{features::kGlicContextMenuBelowSearch});
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
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Search Google option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("https://www.google.com/search?q={searchTerms}");
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
                               features::kDesktopGlowUp,
                               features::kGlicContextMenuBelowSearch});
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
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Search Google option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("https://www.google.com/search?q={searchTerms}");
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
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Search Google option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("https://www.google.com/search?q={searchTerms}");
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

IN_PROC_BROWSER_TEST_F(GlicContextMenuStandardBrowserTest,
                       GlicItemPresentForLinkWithTextSelection) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.link_url = GURL("https://example.com");
  params.unfiltered_link_url = GURL("https://example.com");
  params.selection_text = u"test query";

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

IN_PROC_BROWSER_TEST_F(GlicTextSelectionContextMenuBrowserTest,
                       GlicItemSanitizesSelectionText) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.selection_text = u"line1\u2028line2\u2029line3\r\nline4";

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
  auto glic_index = menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC);
  ASSERT_TRUE(glic_index.has_value());
  EXPECT_EQ(glic_index->first->GetLabelAt(glic_index->second),
            l10n_util::GetStringFUTF16(IDS_GLIC_CONTEXT_MENU_ASK_GEMINI_ABOUT,
                                       u"line1 line2 line3  line4"));
}

class GlicTextSelectionContextMenuAutoSubmitBrowserTest
    : public GlicContextMenuBrowserTestBase {
 public:
  GlicTextSelectionContextMenuAutoSubmitBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlicTextSelectionContextMenu,
          {{features::kGlicTextSelectionContextMenuAutoSubmit.name, "true"}}}},
        // kGlicContextMenu is disabled so the item can only come from the
        // text selection entry point, which shows it on its own (see
        // GlicEnabling::IsContextualMenuItemEnabled()).
        /*disabled_features=*/{features::kGlicContextMenu});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the auto-submit arm labels the item with both the page and the
// selected text.
IN_PROC_BROWSER_TEST_F(GlicTextSelectionContextMenuAutoSubmitBrowserTest,
                       UsesPageAndSelectionLabel) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::ContextMenuParams params;
  params.page_url = web_contents->GetVisibleURL();
  params.selection_text = u"valid text";

  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *web_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
  auto glic_index = menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC);
  ASSERT_TRUE(glic_index.has_value());
  EXPECT_EQ(glic_index->first->GetLabelAt(glic_index->second),
            l10n_util::GetStringFUTF16(
                IDS_GLIC_CONTEXT_MENU_ASK_GEMINI_ABOUT_PAGE_AND_SELECTION,
                u"valid text"));
}

class GlicContextMenuBelowSearchBrowserTest
    : public GlicContextMenuBrowserTestBase {
 public:
  GlicContextMenuBelowSearchBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kGlicContextMenu,
                              features::kGlicShareImage,
                              features::kGlicContextMenuBelowSearch},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the Glic item follows the default search provider in the text
// selection context menu when kGlicContextMenuBelowSearch is enabled.
IN_PROC_BROWSER_TEST_F(GlicContextMenuBelowSearchBrowserTest,
                       GlicItemFollowsSearchProvider) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Search Google option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("https://www.google.com/search?q={searchTerms}");
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
  EXPECT_GT(glic_index->second, search_index->second);
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuBelowSearchBrowserTest,
                       GlicItemFollowsSearchProviderInEditableField) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Search Google option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("https://www.google.com/search?q={searchTerms}");
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
  EXPECT_GT(glic_index->second, search_index->second);
}

IN_PROC_BROWSER_TEST_F(GlicContextMenuBelowSearchBrowserTest,
                       GlicItemFollowsLensInEmptyPage) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(model, nullptr);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);

  // Set up default search provider to enable Lens region search option.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("https://www.google.com/search?q={searchTerms}");
  data.image_url = "https://www.google.com/searchbyimage/upload";
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

  auto glic_index = menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC);
  auto lens_index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH);

  ASSERT_TRUE(glic_index.has_value());
  ASSERT_TRUE(lens_index.has_value());
  EXPECT_EQ(glic_index->first, lens_index->first);
  EXPECT_GT(glic_index->second, lens_index->second);
}

class GlicInternalContextMenuBrowserTest
    : public GlicBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  GlicInternalContextMenuBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kGlic, features::kGlicArchiveConversation};
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsNoWebview()) {
      enabled_features.push_back(features::kGlicNoWebview);
    } else {
      disabled_features.push_back(features::kGlicNoWebview);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool IsNoWebview() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         GlicInternalContextMenuBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NoWebview" : "Webview";
                         });

IN_PROC_BROWSER_TEST_P(GlicInternalContextMenuBrowserTest,
                       GuestContextMenuShowsCustomItemsAndSuppressesPageItems) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  content::ContextMenuParams params;
  params.page_url = guest_contents->GetVisibleURL();
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *guest_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  // Reload should be present and enabled.
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_RELOAD_GLIC));
  EXPECT_TRUE(menu->IsItemEnabled(IDC_CONTENT_CONTEXT_RELOAD_GLIC));
  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_ARCHIVE_GLIC));

  // Standard page navigation items should be suppressed.
  EXPECT_FALSE(menu->IsItemPresent(IDC_BACK));
  EXPECT_FALSE(menu->IsItemPresent(IDC_FORWARD));
  EXPECT_FALSE(menu->IsItemPresent(IDC_RELOAD));
  EXPECT_FALSE(menu->IsItemPresent(IDC_SAVE_PAGE));
  EXPECT_FALSE(menu->IsItemPresent(IDC_VIEW_SOURCE));

  // Ask Gemini should not appear recursively.
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
}

IN_PROC_BROWSER_TEST_P(GlicInternalContextMenuBrowserTest,
                       GuestLinkContextMenuShowsCopyLinkOnly) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  content::ContextMenuParams params;
  params.page_url = guest_contents->GetVisibleURL();
  params.link_url = GURL("https://example.com");
  params.unfiltered_link_url = GURL("https://example.com");
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *guest_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
}

IN_PROC_BROWSER_TEST_P(GlicInternalContextMenuBrowserTest,
                       GuestContextMenuReloadReloadsContents) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_TRUE(instance->host().web_client_contents());
  base::WeakPtr<content::WebContents> initial_guest_contents =
      instance->host().web_client_contents()->GetWeakPtr();

  content::ContextMenuParams params;
  params.page_url = initial_guest_contents->GetVisibleURL();
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *initial_guest_contents->GetPrimaryMainFrame(), params);
  menu->Init();
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_RELOAD_GLIC));

  GlicClientConnectionObserver connection_observer(instance);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_RELOAD_GLIC, 0);

  ASSERT_OK(connection_observer.WaitForDisconnected());
  ASSERT_OK(connection_observer.WaitForConnected());
  EXPECT_OK(
      WaitForWebUiContentsVisibility(instance, content::Visibility::VISIBLE));
  EXPECT_NE(initial_guest_contents.get(),
            instance->host().web_client_contents());
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
IN_PROC_BROWSER_TEST_P(GlicInternalContextMenuBrowserTest,
                       GuestContextMenuPrintTriggersPrintPreview) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  // Verify that PrintViewManager was attached to the guest WebContents.
  ASSERT_NE(nullptr,
            printing::PrintViewManager::FromWebContents(guest_contents));

  printing::TestPrintViewManagerForRequestPreview::CreateForWebContents(
      guest_contents);
  auto* test_print_manager =
      printing::TestPrintViewManagerForRequestPreview::FromWebContents(
          guest_contents);
  ASSERT_TRUE(test_print_manager);

  base::RunLoop run_loop;
  test_print_manager->set_quit_closure(run_loop.QuitClosure());

  content::ContextMenuParams params;
  params.page_url = guest_contents->GetVisibleURL();
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *guest_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_PRINT));
  EXPECT_TRUE(menu->IsItemEnabled(IDC_PRINT));

  menu->ExecuteCommand(IDC_PRINT, 0);

  run_loop.Run();
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

IN_PROC_BROWSER_TEST_P(GlicInternalContextMenuBrowserTest,
                       GuestContextMenuInspectElementOpensDevTools) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  content::ContextMenuParams params;
  params.page_url = guest_contents->GetVisibleURL();
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *guest_contents->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  EXPECT_TRUE(menu->IsItemEnabled(IDC_CONTENT_CONTEXT_INSPECTELEMENT));

  DevToolsWindowCreationObserver observer;
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_INSPECTELEMENT, 0);
  observer.WaitForLoad();
  DevToolsWindow* window = observer.devtools_window();
  EXPECT_TRUE(window);
  observer.CloseAllSync();
}

class GlicNoWebviewOverlayContextMenuBrowserTest : public GlicBrowserTest {
 public:
  GlicNoWebviewOverlayContextMenuBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicNoWebview}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicNoWebviewOverlayContextMenuBrowserTest,
                       OverlayContextMenuShowsReloadAndSuppressesPageItems) {
  GlicNoWebviewContentsManager manager(GetProfile(), &service()->enabling(),
                                       /*initially_hidden=*/false);
  manager.SetVisibility(content::Visibility::VISIBLE);
  content::WebContents* overlay = manager.overlay_contents();
  ASSERT_TRUE(overlay);

  content::ContextMenuParams params;
  params.page_url = overlay->GetVisibleURL();
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *overlay->GetPrimaryMainFrame(), params);
  menu->Init();

  EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_RELOAD_GLIC));
  EXPECT_FALSE(menu->IsItemPresent(IDC_BACK));
  EXPECT_FALSE(menu->IsItemPresent(IDC_FORWARD));
  EXPECT_FALSE(menu->IsItemPresent(IDC_RELOAD));
  EXPECT_FALSE(menu->IsItemPresent(IDC_SAVE_PAGE));
  EXPECT_FALSE(menu->IsItemPresent(IDC_VIEW_SOURCE));
  EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC));
}

class GlicSmartSuggestionContextMenuBrowserTest
    : public GlicContextMenuBrowserTestBase {
 public:
  GlicSmartSuggestionContextMenuBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kGlicContextMenu,
                              features::kGlicSuggestionContextMenu,
                              features::kGlicSelectionOverlayPromptBox},
        /*disabled_features=*/{features::kGlicSelectionPrompt});
  }

 protected:
  std::unique_ptr<TestRenderViewContextMenu> CreateContextMenuWithParams(
      content::ContextMenuParams params) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    params.page_url = web_contents->GetVisibleURL();
    auto menu = std::make_unique<TestRenderViewContextMenu>(
        *web_contents->GetPrimaryMainFrame(), params);
    menu->Init();
    return menu;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicSmartSuggestionContextMenuBrowserTest,
                       ItemPresentAndOpensOverlay) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  auto* controller = SelectionOverlayController::FromTabWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(controller);

  auto menu = CreateContextMenu();
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION));
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION,
                       /*event_flags=*/0);
  histogram_tester.ExpectUniqueSample("SmartSelection.Invocation.ByEntryPoint",
                                      ::selection::EntryPoint::kContextMenuPage,
                                      1);

  ASSERT_OK(RunUntilEqual([&]() { return controller->state(); },
                          SelectionOverlayController::State::kOverlay,
                          "Timed out waiting for the selection overlay."));
  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);
  content::RenderFrameHost* overlay_frame = controller->GetOverlayMainFrame();
  ASSERT_TRUE(overlay_frame);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(overlay_frame, R"(
      (() => {
        const app = document.querySelector('selection-overlay-app');
        const overlay =
            app?.shadowRoot?.querySelector('glic-selection-overlay');
        return !!app?.screenshot_ && !!overlay &&
               !overlay.hideHandles && !overlay.skipIntroAnimation;
      })();
    )")
        .ExtractBool();
  }));
}

IN_PROC_BROWSER_TEST_F(GlicSmartSuggestionContextMenuBrowserTest,
                       ExecWithTextSelectionPreselectsRegionAndSetsOptions) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);

  web_contents->SelectAll();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    std::optional<gfx::Rect> bounds = web_contents->GetTextSelectionBounds(
        web_contents->GetPrimaryMainFrame());
    return bounds.has_value() && !bounds->IsEmpty();
  }));

  content::ContextMenuParams params;
  // Only needs to be non-empty; the bounds come from the real selection above.
  params.selection_text = u"OK";
  auto menu = CreateContextMenuWithParams(params);
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION));
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION,
                       /*event_flags=*/0);
  histogram_tester.ExpectUniqueSample("SmartSelection.Invocation.ByEntryPoint",
                                      ::selection::EntryPoint::kContextMenuText,
                                      1);

  ASSERT_OK(RunUntilEqual([&]() { return controller->state(); },
                          SelectionOverlayController::State::kOverlay,
                          "Timed out waiting for the selection overlay."));
  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);
  content::RenderFrameHost* overlay_frame = controller->GetOverlayMainFrame();
  ASSERT_TRUE(overlay_frame);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(overlay_frame, R"(
      (() => {
        const app = document.querySelector('selection-overlay-app');
        const overlay =
            app?.shadowRoot?.querySelector('glic-selection-overlay');
        return !!app?.screenshot_ && !!overlay &&
               overlay.hideHandles && overlay.skipIntroAnimation;
      })();
    )")
        .ExtractBool();
  }));
}

IN_PROC_BROWSER_TEST_F(GlicSmartSuggestionContextMenuBrowserTest,
                       ExecWithImagePreselectsRegionAndSetsOptions) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);

  ASSERT_TRUE(content::EvalJs(web_contents, R"(
    new Promise((resolve) => {
      const img = document.createElement('img');
      img.style.cssText =
          'position: fixed; left: 20px; top: 30px; width: 100px; height: 80px;';
      img.onload = () => resolve(true);
      img.src = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==';
      document.body.appendChild(img);
    })
  )")
                  .ExtractBool());
  content::WaitForCopyableViewInWebContents(web_contents);

  ContextMenuNotificationObserver menu_observer(
      IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION);
  content::SimulateMouseClickAt(web_contents, 0,
                                blink::WebMouseEvent::Button::kRight,
                                gfx::Point(50, 50));

  ASSERT_OK(RunUntilEqual([&]() { return controller->state(); },
                          SelectionOverlayController::State::kOverlay,
                          "Timed out waiting for the selection overlay."));
  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);
  content::RenderFrameHost* overlay_frame = controller->GetOverlayMainFrame();
  ASSERT_TRUE(overlay_frame);
  gfx::Rect view_bounds = web_contents->GetViewBounds();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(overlay_frame,
                           content::JsReplace(
                               R"(
      (() => {
        const app = document.querySelector('selection-overlay-app');
        const overlay =
            app?.shadowRoot?.querySelector('glic-selection-overlay');
        const sel = overlay?.activeSelection;
        if (!app?.screenshot_ || !overlay || !sel) {
          return false;
        }
        const expectedLeft = 20 / $1;
        const expectedTop = 30 / $2;
        const expectedWidth = 100 / $1;
        const expectedHeight = 80 / $2;
        return !overlay.hideHandles && overlay.skipIntroAnimation &&
               Math.abs(sel.left - expectedLeft) < 0.01 &&
               Math.abs(sel.top - expectedTop) < 0.01 &&
               Math.abs(sel.width - expectedWidth) < 0.01 &&
               Math.abs(sel.height - expectedHeight) < 0.01;
      })();
    )",
                               view_bounds.width(), view_bounds.height()))
        .ExtractBool();
  }));
}

IN_PROC_BROWSER_TEST_F(GlicSmartSuggestionContextMenuBrowserTest,
                       ItemPresentForTextSelectionAndSanitizesLabel) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::ContextMenuParams params;
  params.selection_text = u"  line1\u2028line2\r\nline3  ";
  auto menu = CreateContextMenuWithParams(params);
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION));

  auto index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION);
  ASSERT_TRUE(index.has_value());
  EXPECT_EQ(
      index->first->GetLabelAt(index->second),
      l10n_util::GetStringFUTF16(IDS_GLIC_CONTEXT_MENU_ASK_ABOUT_SELECTION,
                                 u"line1 line2  line3"));
}

IN_PROC_BROWSER_TEST_F(GlicSmartSuggestionContextMenuBrowserTest,
                       ItemHasNewBadge) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  auto* user_education_service =
      UserEducationServiceFactory::GetForBrowserContext(
          browser()->GetProfile());
  ASSERT_TRUE(user_education_service);
  auto* new_badge_controller = user_education_service->new_badge_controller();
  ASSERT_TRUE(new_badge_controller);
  // New badges are not shown to brand-new profiles.
  user_education_service->user_education_storage_service()
      .set_profile_creation_time_for_testing(base::Time::Now() -
                                             base::Days(100));
  new_badge_controller->InitData();

  auto menu = CreateContextMenu();
  auto index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION);
  ASSERT_TRUE(index.has_value());
  EXPECT_TRUE(index->first->IsNewFeatureAt(index->second));
}

IN_PROC_BROWSER_TEST_F(GlicSmartSuggestionContextMenuBrowserTest,
                       ItemPresentInEditableFieldWithSelection) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::ContextMenuParams params;
  params.selection_text = u"selected text";
  params.is_editable = true;
  params.form_control_type = blink::mojom::FormControlType::kInputText;
  auto menu = CreateContextMenuWithParams(params);
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION));

  auto index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION);
  ASSERT_TRUE(index.has_value());
  EXPECT_EQ(index->second, 0u);
  EXPECT_EQ(index->first->GetLabelAt(index->second),
            l10n_util::GetStringFUTF16(
                IDS_GLIC_CONTEXT_MENU_ASK_ABOUT_SELECTION, u"selected text"));

  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION,
                       /*event_flags=*/0);
  auto* selection_observer =
      GlicSelectionObserver::From(browser()->tab_strip_model()->GetActiveTab());
  ASSERT_TRUE(selection_observer);
  EXPECT_EQ(selection_observer->GetSelectedText(), u"selected text");

  // Password fields with a selection should not show the item.
  content::ContextMenuParams password_params;
  password_params.selection_text = u"selected text";
  password_params.is_editable = true;
  password_params.form_control_type =
      blink::mojom::FormControlType::kInputPassword;
  EXPECT_FALSE(CreateContextMenuWithParams(password_params)
                   ->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION));
}

IN_PROC_BROWSER_TEST_F(GlicSmartSuggestionContextMenuBrowserTest,
                       ItemPresentInEditableFieldWithoutSelection) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::ContextMenuParams params;
  params.is_editable = true;
  params.form_control_type = blink::mojom::FormControlType::kInputText;
  params.node_bounds = gfx::Rect(10, 10, 100, 30);
  auto menu = CreateContextMenuWithParams(params);
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION));

  auto index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION);
  ASSERT_TRUE(index.has_value());
  EXPECT_EQ(index->second, 0u);
  EXPECT_EQ(index->first->GetLabelAt(index->second),
            l10n_util::GetStringUTF16(IDS_GLIC_CONTEXT_MENU_ASK_ABOUT_THIS));

  // Password fields should not show the item.
  content::ContextMenuParams password_params;
  password_params.is_editable = true;
  password_params.form_control_type =
      blink::mojom::FormControlType::kInputPassword;
  password_params.node_bounds = gfx::Rect(10, 10, 100, 30);
  EXPECT_FALSE(CreateContextMenuWithParams(password_params)
                   ->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION));
}

IN_PROC_BROWSER_TEST_F(GlicSmartSuggestionContextMenuBrowserTest,
                       ItemPresentForImage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::ContextMenuParams params;
  params.has_image_contents = true;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kImage;
  params.node_bounds = gfx::Rect(10, 10, 100, 100);
  auto menu = CreateContextMenuWithParams(params);
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION));
  auto index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION);
  ASSERT_TRUE(index.has_value());
  EXPECT_EQ(index->second, 0u);
  EXPECT_EQ(index->first->GetLabelAt(index->second),
            l10n_util::GetStringUTF16(IDS_GLIC_CONTEXT_MENU_ASK_ABOUT_THIS));

  // Broken/unloaded images without image contents should not show the item.
  content::ContextMenuParams broken_image_params;
  broken_image_params.has_image_contents = false;
  broken_image_params.media_type =
      blink::mojom::ContextMenuDataMediaType::kImage;
  broken_image_params.node_bounds = gfx::Rect(10, 10, 100, 100);
  EXPECT_FALSE(CreateContextMenuWithParams(broken_image_params)
                   ->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION));
}

IN_PROC_BROWSER_TEST_F(GlicSmartSuggestionContextMenuBrowserTest,
                       ItemPresentBelowOpenLinkGroupForLink) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));

  content::ContextMenuParams link_params;
  link_params.link_url = GURL("https://example.com");
  link_params.unfiltered_link_url = GURL("https://example.com");
  link_params.node_bounds = gfx::Rect(10, 10, 100, 20);
  auto menu = CreateContextMenuWithParams(link_params);
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION));

  auto glic_index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION);
  auto open_tab_index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  auto save_link_index =
      menu->GetMenuModelAndItemIndex(IDC_CONTENT_CONTEXT_SAVELINKAS);
  ASSERT_TRUE(glic_index.has_value());
  ASSERT_TRUE(open_tab_index.has_value());
  ASSERT_TRUE(save_link_index.has_value());
  EXPECT_GT(glic_index->second, open_tab_index->second);
  EXPECT_LT(glic_index->second, save_link_index->second);
  EXPECT_EQ(glic_index->first->GetLabelAt(glic_index->second),
            l10n_util::GetStringUTF16(IDS_GLIC_CONTEXT_MENU_ASK_ABOUT_THIS));
}

IN_PROC_BROWSER_TEST_F(GlicSmartSuggestionContextMenuBrowserTest,
                       ItemPresentForPlugin) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  const GURL page_url = browser()
                            ->tab_strip_model()
                            ->GetActiveWebContents()
                            ->GetLastCommittedURL();

  // Full-page plugin shows "Ask about this page".
  content::ContextMenuParams plugin_params;
  plugin_params.media_type = blink::mojom::ContextMenuDataMediaType::kPlugin;
  plugin_params.src_url = page_url;
  auto plugin_menu = CreateContextMenuWithParams(plugin_params);
  ASSERT_TRUE(
      plugin_menu->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION));
  auto plugin_index = plugin_menu->GetMenuModelAndItemIndex(
      IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION);
  ASSERT_TRUE(plugin_index.has_value());
  EXPECT_EQ(
      plugin_index->first->GetLabelAt(plugin_index->second),
      l10n_util::GetStringUTF16(IDS_GLIC_CONTEXT_MENU_ASK_ABOUT_THIS_PAGE));

  // Embedded non-full-page plugin should not show the item.
  content::ContextMenuParams embedded_plugin_params;
  embedded_plugin_params.media_type =
      blink::mojom::ContextMenuDataMediaType::kPlugin;
  embedded_plugin_params.src_url = GURL("https://example.com/embedded.pdf");
  EXPECT_FALSE(CreateContextMenuWithParams(embedded_plugin_params)
                   ->IsItemPresent(IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION));
}

IN_PROC_BROWSER_TEST_F(GlicSmartSuggestionContextMenuBrowserTest,
                       ExecWithLinkPreselectsRegionAndSetsOptions) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);

  ASSERT_TRUE(content::ExecJs(web_contents, R"(
    const a = document.createElement('a');
    a.href = 'https://example.com';
    a.style.cssText =
        'position: fixed; left: 20px; top: 30px; width: 100px; height: 80px; display: block;';
    a.innerHTML =
        '<svg width="100" height="80"><rect width="100" height="80" fill="blue"/></svg>';
    document.body.appendChild(a);
  )"));
  content::WaitForCopyableViewInWebContents(web_contents);

  ContextMenuNotificationObserver menu_observer(
      IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION);
  content::SimulateMouseClickAt(web_contents, 0,
                                blink::WebMouseEvent::Button::kRight,
                                gfx::Point(50, 50));

  ASSERT_OK(RunUntilEqual([&]() { return controller->state(); },
                          SelectionOverlayController::State::kOverlay,
                          "Timed out waiting for the selection overlay."));
  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);
  content::RenderFrameHost* overlay_frame = controller->GetOverlayMainFrame();
  ASSERT_TRUE(overlay_frame);
  gfx::Rect view_bounds = web_contents->GetViewBounds();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(overlay_frame,
                           content::JsReplace(
                               R"(
      (() => {
        const app = document.querySelector('selection-overlay-app');
        const overlay =
            app?.shadowRoot?.querySelector('glic-selection-overlay');
        const sel = overlay?.activeSelection;
        if (!app?.screenshot_ || !overlay || !sel) {
          return false;
        }
        const expectedLeft = 20 / $1;
        const expectedTop = 30 / $2;
        const expectedWidth = 100 / $1;
        const expectedHeight = 80 / $2;
        return !overlay.hideHandles && overlay.skipIntroAnimation &&
               Math.abs(sel.left - expectedLeft) < 0.01 &&
               Math.abs(sel.top - expectedTop) < 0.01 &&
               Math.abs(sel.width - expectedWidth) < 0.01 &&
               Math.abs(sel.height - expectedHeight) < 0.01;
      })();
    )",
                               view_bounds.width(), view_bounds.height()))
        .ExtractBool();
  }));
}

IN_PROC_BROWSER_TEST_F(GlicSmartSuggestionContextMenuBrowserTest,
                       ExecWithInputPreselectsRegionAndSetsOptions) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);

  ASSERT_TRUE(content::ExecJs(web_contents, R"(
    const input = document.createElement('input');
    input.type = 'text';
    input.style.cssText =
        'position: fixed; left: 20px; top: 30px; width: 100px; height: 40px; box-sizing: border-box;';
    document.body.appendChild(input);
  )"));
  content::WaitForCopyableViewInWebContents(web_contents);

  ContextMenuNotificationObserver menu_observer(
      IDC_CONTENT_CONTEXT_GLIC_SMART_SUGGESTION);
  content::SimulateMouseClickAt(web_contents, 0,
                                blink::WebMouseEvent::Button::kRight,
                                gfx::Point(50, 50));

  ASSERT_OK(RunUntilEqual([&]() { return controller->state(); },
                          SelectionOverlayController::State::kOverlay,
                          "Timed out waiting for the selection overlay."));
  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);
  content::RenderFrameHost* overlay_frame = controller->GetOverlayMainFrame();
  ASSERT_TRUE(overlay_frame);
  gfx::Rect view_bounds = web_contents->GetViewBounds();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(overlay_frame,
                           content::JsReplace(
                               R"(
      (() => {
        const app = document.querySelector('selection-overlay-app');
        const overlay =
            app?.shadowRoot?.querySelector('glic-selection-overlay');
        const sel = overlay?.activeSelection;
        if (!app?.screenshot_ || !overlay || !sel) {
          return false;
        }
        const expectedLeft = 20 / $1;
        const expectedTop = 30 / $2;
        const expectedWidth = 100 / $1;
        const expectedHeight = 40 / $2;
        return !overlay.hideHandles && overlay.skipIntroAnimation &&
               Math.abs(sel.left - expectedLeft) < 0.01 &&
               Math.abs(sel.top - expectedTop) < 0.01 &&
               Math.abs(sel.width - expectedWidth) < 0.01 &&
               Math.abs(sel.height - expectedHeight) < 0.01;
      })();
    )",
                               view_bounds.width(), view_bounds.height()))
        .ExtractBool();
  }));
}

class GlicSelectionPromptContextMenuBrowserTest
    : public GlicContextMenuBrowserTestBase {
 public:
  GlicSelectionPromptContextMenuBrowserTest() {
    feature_list_.InitWithFeatures({features::kGlic, features::kGlicContextMenu,
                                    features::kGlicSelectionPrompt},
                                   {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicSelectionPromptContextMenuBrowserTest,
                       ContextMenuDismissesSelectionWidget) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimpleTestUrl()));
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  content::WebContents* web_contents = tab->GetContents();
  ASSERT_TRUE(web_contents);

  auto* selection_observer = GlicSelectionObserver::From(tab);
  ASSERT_TRUE(selection_observer);
  EXPECT_FALSE(selection_observer->IsShowingWidgetForTesting());

  web_contents->Focus();
  web_contents->SelectAll();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return selection_observer->IsShowingWidgetForTesting(); }));

  auto menu = CreateContextMenu();
  EXPECT_FALSE(selection_observer->IsShowingWidgetForTesting());
}

}  // namespace glic
