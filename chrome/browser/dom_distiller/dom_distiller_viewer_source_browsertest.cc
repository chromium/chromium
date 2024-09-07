// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/browser/dom_distiller_viewer_source.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/fake_distiller.h"
#include "components/dom_distiller/core/fake_distiller_page.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace dom_distiller {

using test::FakeDistiller;
using test::MockDistillerFactory;
using test::MockDistillerPage;
using test::MockDistillerPageFactory;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::HasSubstr;
using testing::Not;

namespace {

const char kGetLoadIndicatorClassName[] =
    "document.getElementById('loading-indicator').className";

const char kGetContent[] = "document.getElementById('content').innerHTML";

const char kGetTitle[] = "document.title";

const char kGetBodyClass[] = "document.body.className";

const char kGetFontSize[] =
    "window.getComputedStyle(document.documentElement).fontSize";

const unsigned kDarkToolbarThemeColor = 0xFF1A1A1A;

const char kTestDistillerObject[] = "typeof distiller == 'object'";

void ExpectBodyHasThemeAndFont(content::WebContents* contents,
                               const std::string& expected_theme,
                               const std::string& expected_font) {
  std::string result = content::EvalJs(contents, kGetBodyClass).ExtractString();
  EXPECT_THAT(result, HasSubstr(expected_theme));
  EXPECT_THAT(result, HasSubstr(expected_font));
}

class PrefChangeObserver : public DistilledPagePrefs::Observer {
 public:
  void WaitForChange(DistilledPagePrefs* prefs) {
    prefs->AddObserver(this);
    base::RunLoop run_loop;
    callback_ = run_loop.QuitClosure();
    run_loop.Run();
    prefs->RemoveObserver(this);
  }

  void OnChangeFontFamily(mojom::FontFamily font_family) override {
    callback_.Run();
  }
  void OnChangeTheme(mojom::Theme theme) override { callback_.Run(); }
  void OnChangeFontScaling(float scaling) override { callback_.Run(); }

 private:
  base::RepeatingClosure callback_;
};

}  // namespace

class DomDistillerViewerSourceBrowserTest : public InProcessBrowserTest {
 public:
  DomDistillerViewerSourceBrowserTest() {}
  ~DomDistillerViewerSourceBrowserTest() override {}

  void SetUpOnMainThread() override {
    if (!DistillerJavaScriptWorldIdIsSet()) {
      SetDistillerJavaScriptWorldId(content::ISOLATED_WORLD_ID_CONTENT_END);
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableDomDistiller);
  }

  std::unique_ptr<KeyedService> Build(content::BrowserContext* context) {
    auto distiller_factory = std::make_unique<MockDistillerFactory>();
    distiller_factory_ = distiller_factory.get();
    auto distiller_page_factory = std::make_unique<MockDistillerPageFactory>();
    auto* distiller_page_factory_raw = distiller_page_factory.get();
    auto service = std::make_unique<DomDistillerContextKeyedService>(
        std::move(distiller_factory), std::move(distiller_page_factory),
        std::make_unique<DistilledPagePrefs>(
            Profile::FromBrowserContext(context)->GetPrefs()),
        /* distiller_ui_handle */ nullptr);
    if (expect_distillation_) {
      // There will only be destillation of an article if the database contains
      // the article.
      FakeDistiller* distiller = new FakeDistiller(true);
      EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
          .WillOnce(testing::Return(distiller));
    }
    if (expect_distiller_page_) {
      MockDistillerPage* distiller_page = new MockDistillerPage();
      EXPECT_CALL(*distiller_page_factory_raw, CreateDistillerPageImpl())
          .WillOnce(testing::Return(distiller_page));
    }
    return std::move(service);
  }

  void ViewSingleDistilledPage(const GURL& url,
                               const std::string& expected_mime_type);
  void ViewSingleDistilledPageAndExpectErrorPage(const GURL& url);
  void PrefTest(bool is_error_page);

  // Database entries.
  bool expect_distillation_ = false;
  bool expect_distiller_page_ = false;
  raw_ptr<MockDistillerFactory, DanglingUntriaged> distiller_factory_ = nullptr;
};

// The DomDistillerViewerSource renders untrusted content, so ensure no bindings
// are enabled when the article is not found.
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       NoWebUIBindingsArticleNotFound) {
  // The article does not exist, so assume no distillation will happen.
  expect_distillation_ = false;
  expect_distiller_page_ = false;
  const GURL url = url_utils::GetDistillerViewUrlFromEntryId(
      kDomDistillerScheme, "DOES_NOT_EXIST");
  ViewSingleDistilledPage(url, "text/html");
}

// The DomDistillerViewerSource renders untrusted content, so ensure no bindings
// are enabled when requesting to view an arbitrary URL.
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       NoWebUIBindingsViewUrl) {
  // We should expect distillation for any valid URL.
  expect_distillation_ = true;
  expect_distiller_page_ = true;
  GURL original_url("http://www.example.com/1");
  const GURL view_url = url_utils::GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, original_url, "Title");
  ViewSingleDistilledPage(view_url, "text/html");
}

void DomDistillerViewerSourceBrowserTest::ViewSingleDistilledPage(
    const GURL& url,
    const std::string& expected_mime_type) {
  // Ensure the correct factory is used for the DomDistillerService.
  dom_distiller::DomDistillerServiceFactory::GetInstance()
      ->SetTestingFactoryAndUse(
          browser()->profile(),
          base::BindRepeating(&DomDistillerViewerSourceBrowserTest::Build,
                              base::Unretained(this)));

  // Navigate to a URL which the source should respond to.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Ensure no bindings for the loaded |url|.
  content::WebContents* contents_after_nav =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents_after_nav != nullptr);
  EXPECT_EQ(url, contents_after_nav->GetLastCommittedURL());
  content::RenderFrameHost* render_frame_host =
      contents_after_nav->GetPrimaryMainFrame();
  EXPECT_TRUE(render_frame_host->GetEnabledBindings().empty());
  EXPECT_EQ(expected_mime_type, contents_after_nav->GetContentsMimeType());
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       TestBadUrlErrorPage) {
  GURL url("chrome-distiller://bad");
  ViewSingleDistilledPageAndExpectErrorPage(url);
}

void DomDistillerViewerSourceBrowserTest::
    ViewSingleDistilledPageAndExpectErrorPage(const GURL& url) {
  // Navigate to a distiller URL.
  ViewSingleDistilledPage(url, "text/html");
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait for the page load to complete as the first page completes the root
  // document.
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  ASSERT_TRUE(contents != nullptr);
  EXPECT_EQ(url, contents->GetLastCommittedURL());

  std::string result = content::EvalJs(contents, kGetContent).ExtractString();
  EXPECT_THAT(result,
              HasSubstr(l10n_util::GetStringUTF8(
                  IDS_DOM_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_CONTENT)));
  result = content::EvalJs(contents, kGetTitle).ExtractString();
  EXPECT_THAT(result,
              HasSubstr(l10n_util::GetStringUTF8(
                  IDS_DOM_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_TITLE)));
}

// The DomDistillerViewerSource renders untrusted content, so ensure no bindings
// are enabled when the CSS resource is loaded. This CSS might be bundle with
// Chrome or provided by an extension.
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       NoWebUIBindingsDisplayCSS) {
  expect_distillation_ = false;
  expect_distiller_page_ = false;
  // Navigate to a URL which the source should respond to with CSS.
  std::string url_without_scheme = std::string("://foobar/") + kViewerCssPath;
  GURL url(kDomDistillerScheme + url_without_scheme);
  ViewSingleDistilledPage(url, "text/css");
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       EmptyURLShouldNotCrash) {
  // This is a bogus URL, so no distillation will happen.
  expect_distillation_ = false;
  expect_distiller_page_ = false;
  const GURL url(std::string(kDomDistillerScheme) + "://bogus/");
  ViewSingleDistilledPage(url, "text/html");
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       InvalidURLShouldNotCrash) {
  // This is a bogus URL, so no distillation will happen.
  expect_distillation_ = false;
  expect_distiller_page_ = false;
  const GURL url(std::string(kDomDistillerScheme) + "://bogus/foobar");
  ViewSingleDistilledPage(url, "text/html");
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       InvalidURLShouldGetErrorPage) {
  const GURL original_url("http://www.example.com/1");
  const GURL different_url("http://www.example.com/2");
  const GURL view_url = url_utils::GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, original_url, "Title");
  // This is a bogus URL, so no distillation will happen.
  const GURL bad_view_url = net::AppendOrReplaceQueryParameter(
      view_url, kUrlKey, different_url.spec());

  ViewSingleDistilledPageAndExpectErrorPage(bad_view_url);
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest, EarlyTemplateLoad) {
  dom_distiller::DomDistillerServiceFactory::GetInstance()
      ->SetTestingFactoryAndUse(
          browser()->profile(),
          base::BindRepeating(&DomDistillerViewerSourceBrowserTest::Build,
                              base::Unretained(this)));

  base::RunLoop distillation_done_loop;

  FakeDistiller* distiller =
      new FakeDistiller(false, distillation_done_loop.QuitClosure());
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(testing::Return(distiller));

  // Navigate to a URL.
  GURL url(dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, GURL("http://urlthatlooksvalid.com"), "Title"));
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  Navigate(&params);
  distillation_done_loop.Run();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait for the page load to complete (should only be template).
  EXPECT_TRUE(content::WaitForLoadStop(contents));
  // Loading spinner should be on screen at this point.
  std::string result =
      content::EvalJs(contents, kGetLoadIndicatorClassName).ExtractString();
  EXPECT_EQ("visible", result);

  result = content::EvalJs(contents, kGetContent).ExtractString();
  EXPECT_THAT(result, Not(HasSubstr("content")));

  // Finish distillation and make sure the spinner has been replaced by text.
  std::vector<scoped_refptr<ArticleDistillationUpdate::RefCountedPageProto>>
      update_pages;
  std::unique_ptr<DistilledArticleProto> article(new DistilledArticleProto());

  scoped_refptr<base::RefCountedData<DistilledPageProto>> page_proto =
      new base::RefCountedData<DistilledPageProto>();
  page_proto->data.set_url("http://foo.html");
  page_proto->data.set_html("<div>content</div>");
  update_pages.push_back(page_proto);
  *(article->add_pages()) = page_proto->data;

  ArticleDistillationUpdate update(update_pages, true, false);
  distiller->RunDistillerUpdateCallback(update);

  EXPECT_TRUE(content::WaitForLoadStop(contents));

  result = content::EvalJs(contents, kGetContent).ExtractString();
  EXPECT_THAT(result, HasSubstr("content"));
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       DISABLED_DistillerJavaScriptExposed) {
  // Navigate to a distiller URL.
  GURL url(std::string(kDomDistillerScheme) + "://url");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  Navigate(&params);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait for the page load to complete (this will be a distiller error page).
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  // Execute in isolated world; where all distiller scripts are run.
  EXPECT_EQ(true, content::EvalJs(contents, kTestDistillerObject,
                                  content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                  ISOLATED_WORLD_ID_CHROME_INTERNAL));
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       DistillerJavaScriptNotInMainWorld) {
  // Navigate to a distiller URL.
  GURL url(std::string(kDomDistillerScheme) + "://url");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  Navigate(&params);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait for the page load to complete (this will be a distiller error page).
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  // Execute in main world, the distiller object should not be here.
  EXPECT_EQ(false, content::EvalJs(contents, kTestDistillerObject));
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       DistillerJavaScriptNotExposed) {
  // Navigate to a non-distiller URL.
  GURL url("http://url");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  Navigate(&params);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait for the page load to complete.
  EXPECT_FALSE(content::WaitForLoadStop(contents));

  EXPECT_EQ(false, content::EvalJs(contents, kTestDistillerObject));
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest, MultiPageArticle) {
  expect_distillation_ = false;
  expect_distiller_page_ = true;
  dom_distiller::DomDistillerServiceFactory::GetInstance()
      ->SetTestingFactoryAndUse(
          browser()->profile(),
          base::BindRepeating(&DomDistillerViewerSourceBrowserTest::Build,
                              base::Unretained(this)));

  base::RunLoop distillation_done_loop;

  FakeDistiller* distiller =
      new FakeDistiller(false, distillation_done_loop.QuitClosure());
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(testing::Return(distiller));

  // Setup observer to inspect the RenderViewHost after committed navigation.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a URL and wait for the distiller to flush contents to the page.
  GURL url(dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, GURL("http://urlthatlooksvalid.com"), "Title"));
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  Navigate(&params);
  distillation_done_loop.Run();

  // Fake a multi-page response from distiller.

  std::vector<scoped_refptr<ArticleDistillationUpdate::RefCountedPageProto>>
      update_pages;
  std::unique_ptr<DistilledArticleProto> article(new DistilledArticleProto());

  // Flush page 1.
  {
    scoped_refptr<base::RefCountedData<DistilledPageProto>> page_proto =
        new base::RefCountedData<DistilledPageProto>();
    page_proto->data.set_url("http://foobar.1.html");
    page_proto->data.set_html("<div>Page 1 content</div>");
    update_pages.push_back(page_proto);
    *(article->add_pages()) = page_proto->data;

    ArticleDistillationUpdate update(update_pages, true, false);
    distiller->RunDistillerUpdateCallback(update);

    // Wait for the page load to complete as the first page completes the root
    // document.
    EXPECT_TRUE(content::WaitForLoadStop(contents));

    std::string result =
        content::EvalJs(contents, kGetLoadIndicatorClassName).ExtractString();
    EXPECT_EQ("visible", result);

    result = content::EvalJs(contents, kGetContent).ExtractString();
    EXPECT_THAT(result, HasSubstr("Page 1 content"));
    EXPECT_THAT(result, Not(HasSubstr("Page 2 content")));
  }

  // Flush page 2.
  {
    scoped_refptr<base::RefCountedData<DistilledPageProto>> page_proto =
        new base::RefCountedData<DistilledPageProto>();
    page_proto->data.set_url("http://foobar.2.html");
    page_proto->data.set_html("<div>Page 2 content</div>");
    update_pages.push_back(page_proto);
    *(article->add_pages()) = page_proto->data;

    ArticleDistillationUpdate update(update_pages, false, false);
    distiller->RunDistillerUpdateCallback(update);

    std::string result =
        content::EvalJs(contents, kGetLoadIndicatorClassName).ExtractString();
    EXPECT_EQ("hidden", result);

    result = content::EvalJs(contents, kGetContent).ExtractString();
    EXPECT_THAT(result, HasSubstr("Page 1 content"));
    EXPECT_THAT(result, HasSubstr("Page 2 content"));
  }

  // Complete the load.
  distiller->RunDistillerCallback(std::move(article));
  base::RunLoop().RunUntilIdle();

  std::string result =
      content::EvalJs(contents, kGetLoadIndicatorClassName).ExtractString();
  EXPECT_EQ("hidden", result);
  result = content::EvalJs(contents, kGetContent).ExtractString();
  EXPECT_THAT(result, HasSubstr("Page 1 content"));
  EXPECT_THAT(result, HasSubstr("Page 2 content"));
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest, PrefChange) {
  PrefTest(false);
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest, PrefChangeError) {
  PrefTest(true);
}

void DomDistillerViewerSourceBrowserTest::PrefTest(bool is_error_page) {
  GURL view_url;
  if (is_error_page) {
    expect_distillation_ = false;
    expect_distiller_page_ = false;
    view_url = GURL("chrome-distiller://bad");
  } else {
    expect_distillation_ = true;
    expect_distiller_page_ = true;
    GURL original_url("http://www.example.com/1");
    view_url = url_utils::GetDistillerViewUrlFromUrl(kDomDistillerScheme,
                                                     original_url, "Title");
  }
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ViewSingleDistilledPage(view_url, "text/html");
  EXPECT_TRUE(content::WaitForLoadStop(contents));
  ExpectBodyHasThemeAndFont(contents, "light", "sans-serif");

  DistilledPagePrefs* distilled_page_prefs =
      DomDistillerServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetDistilledPagePrefs();

  // Test theme.
  distilled_page_prefs->SetTheme(mojom::Theme::kDark);
  base::RunLoop().RunUntilIdle();
  ExpectBodyHasThemeAndFont(contents, "dark", "sans-serif");

  // Verify that the theme color for the tab is updated as well.
  EXPECT_EQ(kDarkToolbarThemeColor, contents->GetThemeColor());

  // Test font family.
  distilled_page_prefs->SetFontFamily(mojom::FontFamily::kMonospace);
  base::RunLoop().RunUntilIdle();
  ExpectBodyHasThemeAndFont(contents, "dark", "monospace");

  // Test font scaling.
  std::string result = content::EvalJs(contents, kGetFontSize).ExtractString();
  double oldFontSize;
  base::StringToDouble(result, &oldFontSize);

  const double kScale = 1.23;
  distilled_page_prefs->SetFontScaling(kScale);
  base::RunLoop().RunUntilIdle();
  result = content::EvalJs(contents, kGetFontSize).ExtractString();
  double fontSize;
  base::StringToDouble(result, &fontSize);
  ASSERT_FLOAT_EQ(kScale, fontSize / oldFontSize);
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest, PrefPersist) {
  expect_distillation_ = false;
  expect_distiller_page_ = false;
  const GURL url("chrome-distiller://bad");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  DistilledPagePrefs* distilled_page_prefs =
      DomDistillerServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetDistilledPagePrefs();

  std::string result = content::EvalJs(contents, kGetFontSize).ExtractString();
  double oldFontSize;
  base::StringToDouble(result, &oldFontSize);

  // Set preference.
  const double kScale = 1.23;
  distilled_page_prefs->SetTheme(mojom::Theme::kDark);
  distilled_page_prefs->SetFontFamily(mojom::FontFamily::kMonospace);
  distilled_page_prefs->SetFontScaling(kScale);

  base::RunLoop().RunUntilIdle();
  ExpectBodyHasThemeAndFont(contents, "dark", "monospace");

  EXPECT_EQ(kDarkToolbarThemeColor, contents->GetThemeColor());
  result = content::EvalJs(contents, kGetFontSize).ExtractString();
  double fontSize;
  base::StringToDouble(result, &fontSize);
  ASSERT_FLOAT_EQ(kScale, fontSize / oldFontSize);

  // Make sure perf persist across web pages.
  GURL url2("chrome-distiller://bad2");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  base::RunLoop().RunUntilIdle();
  result = content::EvalJs(contents, kGetBodyClass).ExtractString();
  ExpectBodyHasThemeAndFont(contents, "dark", "monospace");
  EXPECT_EQ(kDarkToolbarThemeColor, contents->GetThemeColor());

  result = content::EvalJs(contents, kGetFontSize).ExtractString();
  base::StringToDouble(result, &fontSize);
  ASSERT_FLOAT_EQ(kScale, fontSize / oldFontSize);
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest, UISetsPrefs) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Load the distilled page
  GURL view_url;
  expect_distillation_ = true;
  expect_distiller_page_ = true;
  GURL original_url("http://www.example.com/1");
  view_url = url_utils::GetDistillerViewUrlFromUrl(kDomDistillerScheme,
                                                   original_url, "Title");
  ViewSingleDistilledPage(view_url, "text/html");
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  // Wait for all currently executing scripts to finish. Otherwise, the
  // distiller object used to send the prefs to the browser from the JavaScript
  // may not exist, causing test flakiness.
  base::RunLoop().RunUntilIdle();

  DistilledPagePrefs* distilled_page_prefs =
      DomDistillerServiceFactory::GetForBrowserContext(browser()->profile())
          ->GetDistilledPagePrefs();

  // Verify that the initial preferences aren't the same as those set below.
  ExpectBodyHasThemeAndFont(contents, "light", "sans-serif");
  EXPECT_EQ(content::EvalJs(contents, kGetFontSize), "16px");
  EXPECT_NE(mojom::Theme::kDark, distilled_page_prefs->GetTheme());
  EXPECT_NE(mojom::FontFamily::kMonospace,
            distilled_page_prefs->GetFontFamily());
  EXPECT_NE(3.0, distilled_page_prefs->GetFontScaling());

  // 'Click' the associated UI elements for changing each preference.
  const std::string script = R"(
      (() => {
        const observer = new MutationObserver((mutationsList, observer) => {
          const classes = document.body.classList;
          if (classes.contains('dark') && classes.contains('monospace')) {
            observer.disconnect();
            window.domAutomationController.send(document.body.className);
          }
        });

        observer.observe(document.body, {
          attributes: true,
          attributeFilter: [ 'class' ]
        });

        document.querySelector('.theme-option .dark')
          .dispatchEvent(new MouseEvent('click'));
        document.querySelector(
            '#font-family-selection option[value="monospace"]')
          .dispatchEvent(new Event("change", { bubbles: true }));
        const slider = document.getElementById('font-size-selection');
        slider.value = 9;
        slider.dispatchEvent(new Event("input", {bubbles: true}));
      })();)";
  content::DOMMessageQueue queue(contents);
  std::string result;
  content::ExecuteScriptAsync(contents, script);
  ASSERT_TRUE(queue.WaitForMessage(&result));

  // Verify that the preferences changed in the browser-side
  // DistilledPagePrefs.
  PrefChangeObserver observer;
  while (distilled_page_prefs->GetTheme() != mojom::Theme::kDark ||
         mojom::FontFamily::kMonospace !=
             distilled_page_prefs->GetFontFamily() ||
         3.0f != distilled_page_prefs->GetFontScaling()) {
    observer.WaitForChange(distilled_page_prefs);
  }
}

}  // namespace dom_distiller
