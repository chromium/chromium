// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reader_mode/reader_mode_service_factory.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_distiller/content/browser/distiller_javascript_utils.h"
#include "components/web_distiller/content/browser/web_distiller_viewer_source.h"
#include "components/web_distiller/core/article_entry.h"
#include "components/web_distiller/core/distilled_page_prefs.h"
#include "components/web_distiller/core/distiller.h"
#include "components/web_distiller/core/fake_distiller.h"
#include "components/web_distiller/core/fake_distiller_page.h"
#include "components/web_distiller/core/task_tracker.h"
#include "components/web_distiller/core/url_constants.h"
#include "components/web_distiller/core/url_utils.h"
#include "components/web_distiller/core/web_distiller_service.h"
#include "components/web_distiller/core/web_distiller_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"

namespace reader_mode {

using testing::Eq;
using testing::ExplainMatchResult;
using testing::HasSubstr;
using testing::Not;
using web_distiller::DistilledArticleProto;
using web_distiller::DistilledPagePrefs;
using web_distiller::DistilledPageProto;
using web_distiller::DistillerJavaScriptWorldIdIsSet;
using web_distiller::kUrlKey;
using web_distiller::kViewerCssPath;
using web_distiller::kWebDistillerScheme;
using web_distiller::SetDistillerJavaScriptWorldId;
using web_distiller::ThemeSettingsUpdateSource;
using web_distiller::test::FakeDistiller;
using web_distiller::test::MockDistillerFactory;
using web_distiller::test::MockDistillerPage;
using web_distiller::test::MockDistillerPageFactory;

namespace mojom = web_distiller::mojom;
namespace url_utils = web_distiller::url_utils;

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
  void OnChangeTheme(mojom::Theme theme,
                     ThemeSettingsUpdateSource source) override {
    callback_.Run();
  }
  void OnChangeFontScaling(float scaling) override { callback_.Run(); }
  void OnChangeLinksEnabled(bool enabled) override { callback_.Run(); }

 private:
  base::RepeatingClosure callback_;
};

}  // namespace

class ReaderModeViewerSourceBrowserTest : public InProcessBrowserTest {
 public:
  ReaderModeViewerSourceBrowserTest() = default;
  ~ReaderModeViewerSourceBrowserTest() override = default;

  void SetUpOnMainThread() override {
    if (!DistillerJavaScriptWorldIdIsSet()) {
      SetDistillerJavaScriptWorldId(content::ISOLATED_WORLD_ID_CONTENT_END);
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableWebDistiller);
  }

  std::unique_ptr<KeyedService> Build(content::BrowserContext* context) {
    auto distiller_factory = std::make_unique<MockDistillerFactory>();
    distiller_factory_ = distiller_factory.get();
    auto distiller_page_factory = std::make_unique<MockDistillerPageFactory>();
    auto* distiller_page_factory_raw = distiller_page_factory.get();
    auto service = std::make_unique<ReaderModeContextKeyedService>(
        std::move(distiller_factory), std::move(distiller_page_factory),
        std::make_unique<DistilledPagePrefs>(
            Profile::FromBrowserContext(context)->GetPrefs()),
        /* distiller_ui_handle */ nullptr, base::CallbackListSubscription());
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

// The viewer source renders untrusted content, so ensure no bindings are
// enabled when the article is not found.
IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest,
                       NoWebUIBindingsArticleNotFound) {
  // The article does not exist, so assume no distillation will happen.
  expect_distillation_ = false;
  expect_distiller_page_ = false;
  const GURL url = url_utils::GetDistillerViewUrlFromEntryId(
      kWebDistillerScheme, "DOES_NOT_EXIST");
  ViewSingleDistilledPage(url, "text/html");
}

// The viewer source renders untrusted content, so ensure no bindings are
// enabled when requesting to view an arbitrary URL.
IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest,
                       NoWebUIBindingsViewUrl) {
  // We should expect distillation for any valid URL.
  expect_distillation_ = true;
  expect_distiller_page_ = true;
  GURL original_url("http://www.example.com/1");
  const GURL view_url = url_utils::GetDistillerViewUrlFromUrl(
      kWebDistillerScheme, original_url, "Title");
  ViewSingleDistilledPage(view_url, "text/html");
}

void ReaderModeViewerSourceBrowserTest::ViewSingleDistilledPage(
    const GURL& url,
    const std::string& expected_mime_type) {
  // Ensure the correct factory is used for the distiller service.
  ReaderModeServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      browser()->GetProfile(),
      base::BindRepeating(&ReaderModeViewerSourceBrowserTest::Build,
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

IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest, TestBadUrlErrorPage) {
  GURL url("chrome-distiller://bad");
  ViewSingleDistilledPageAndExpectErrorPage(url);
}

void ReaderModeViewerSourceBrowserTest::
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
                  IDS_WEB_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_CONTENT)));
  result = content::EvalJs(contents, kGetTitle).ExtractString();
  EXPECT_THAT(result,
              HasSubstr(l10n_util::GetStringUTF8(
                  IDS_WEB_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_TITLE)));
}

// The viewer source renders untrusted content, so ensure no bindings are
// enabled when the CSS resource is loaded. This CSS might be bundle with Chrome
// or provided by an extension.
IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest,
                       NoWebUIBindingsDisplayCSS) {
  expect_distillation_ = false;
  expect_distiller_page_ = false;
  // Navigate to a URL which the source should respond to with CSS.
  std::string url_without_scheme = std::string("://foobar/") + kViewerCssPath;
  GURL url(kWebDistillerScheme + url_without_scheme);
  ViewSingleDistilledPage(url, "text/css");
}

IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest,
                       EmptyURLShouldNotCrash) {
  // This is a bogus URL, so no distillation will happen.
  expect_distillation_ = false;
  expect_distiller_page_ = false;
  const GURL url(std::string(kWebDistillerScheme) + "://bogus/");
  ViewSingleDistilledPage(url, "text/html");
}

IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest,
                       InvalidURLShouldNotCrash) {
  // This is a bogus URL, so no distillation will happen.
  expect_distillation_ = false;
  expect_distiller_page_ = false;
  const GURL url(std::string(kWebDistillerScheme) + "://bogus/foobar");
  ViewSingleDistilledPage(url, "text/html");
}

IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest,
                       InvalidURLShouldGetErrorPage) {
  const GURL original_url("http://www.example.com/1");
  const GURL different_url("http://www.example.com/2");
  const GURL view_url = url_utils::GetDistillerViewUrlFromUrl(
      kWebDistillerScheme, original_url, "Title");
  // This is a bogus URL, so no distillation will happen.
  const GURL bad_view_url = net::AppendOrReplaceQueryParameter(
      view_url, kUrlKey, different_url.spec());

  ViewSingleDistilledPageAndExpectErrorPage(bad_view_url);
}

IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest, EarlyTemplateLoad) {
  ReaderModeServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      browser()->GetProfile(),
      base::BindRepeating(&ReaderModeViewerSourceBrowserTest::Build,
                          base::Unretained(this)));

  base::RunLoop distillation_done_loop;

  FakeDistiller* distiller =
      new FakeDistiller(false, distillation_done_loop.QuitClosure());
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(testing::Return(distiller));

  // Navigate to a URL.
  GURL url(url_utils::GetDistillerViewUrlFromUrl(
      kWebDistillerScheme, GURL("http://urlthatlooksvalid.com"), "Title"));
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
  auto article = std::make_unique<DistilledArticleProto>();
  DistilledPageProto* page = article->add_pages();
  page->set_url("http://foo.html");
  page->set_html("<div>content</div>");

  distiller->RunDistillerCallback(std::move(article));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(contents, kGetLoadIndicatorClassName)
               .ExtractString() == "hidden";
  }));

  result = content::EvalJs(contents, kGetContent).ExtractString();
  EXPECT_THAT(result, HasSubstr("content"));
}

// Tests that the viewer CSP doesn't block anything a distilled article with
// cross-origin and data: images needs to render.
IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest,
                       NoContentSecurityPolicyViolations) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ReaderModeServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      browser()->GetProfile(),
      base::BindRepeating(&ReaderModeViewerSourceBrowserTest::Build,
                          base::Unretained(this)));

  base::RunLoop distillation_done_loop;
  FakeDistiller* distiller =
      new FakeDistiller(false, distillation_done_loop.QuitClosure());
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(testing::Return(distiller));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetFilter(base::BindRepeating(
      [](const content::WebContentsConsoleObserver::Message& message) {
        return message.message.find(u"Content Security Policy") !=
                   std::u16string::npos ||
               message.message.find(u"Refused to") != std::u16string::npos;
      }));

  GURL url(url_utils::GetDistillerViewUrlFromUrl(
      kWebDistillerScheme, GURL("http://urlthatlooksvalid.com"), "Title"));
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  Navigate(&params);
  distillation_done_loop.Run();
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  const GURL image_url = embedded_test_server()->GetURL("/handbag.png");
  auto article = std::make_unique<DistilledArticleProto>();
  DistilledPageProto* page = article->add_pages();
  page->set_url("http://foo.html");
  page->set_html(base::StrCat(
      {"<div>content<img src=\"", image_url.spec(), "\">",
       "<img src=\"data:image/gif;base64,"
       "R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==\"></div>"}));
  distiller->RunDistillerCallback(std::move(article));

  // Wait for the article and both of its images to finish rendering.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(contents, kGetLoadIndicatorClassName)
               .ExtractString() == "hidden";
  }));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(contents,
                           "(() => { const images = document.querySelectorAll("
                           "'#content img'); return images.length == 2 && "
                           "Array.from(images).every(i => i.complete && "
                           "i.naturalWidth > 0); })()")
        .ExtractBool();
  }));

  EXPECT_TRUE(console_observer.messages().empty())
      << console_observer.GetMessageAt(0);
}

IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest,
                       DISABLED_DistillerJavaScriptExposed) {
  // Navigate to a distiller URL.
  GURL url(std::string(kWebDistillerScheme) + "://url");
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

IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest,
                       DistillerJavaScriptNotInMainWorld) {
  // Navigate to a distiller URL.
  GURL url(std::string(kWebDistillerScheme) + "://url");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  Navigate(&params);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait for the page load to complete (this will be a distiller error page).
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  // Execute in main world, the distiller object should not be here.
  EXPECT_EQ(false, content::EvalJs(contents, kTestDistillerObject));
}

IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest,
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

IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest, PrefChange) {
  PrefTest(false);
}

IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest, PrefChangeError) {
  PrefTest(true);
}

void ReaderModeViewerSourceBrowserTest::PrefTest(bool is_error_page) {
  GURL view_url;
  if (is_error_page) {
    expect_distillation_ = false;
    expect_distiller_page_ = false;
    view_url = GURL("chrome-distiller://bad");
  } else {
    expect_distillation_ = true;
    expect_distiller_page_ = true;
    GURL original_url("http://www.example.com/1");
    view_url = url_utils::GetDistillerViewUrlFromUrl(kWebDistillerScheme,
                                                     original_url, "Title");
  }
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ViewSingleDistilledPage(view_url, "text/html");
  EXPECT_TRUE(content::WaitForLoadStop(contents));
  ExpectBodyHasThemeAndFont(contents, "light", "sans-serif");

  DistilledPagePrefs* distilled_page_prefs =
      ReaderModeServiceFactory::GetForBrowserContext(browser()->GetProfile())
          ->GetDistilledPagePrefs();

  // Test theme.
  distilled_page_prefs->SetUserPrefTheme(mojom::Theme::kDark);
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
  distilled_page_prefs->SetUserPrefFontScaling(kScale);
  base::RunLoop().RunUntilIdle();
  result = content::EvalJs(contents, kGetFontSize).ExtractString();
  double fontSize;
  base::StringToDouble(result, &fontSize);
  ASSERT_FLOAT_EQ(kScale, fontSize / oldFontSize);
}

IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest, PrefPersist) {
  expect_distillation_ = false;
  expect_distiller_page_ = false;
  const GURL url("chrome-distiller://bad");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  DistilledPagePrefs* distilled_page_prefs =
      ReaderModeServiceFactory::GetForBrowserContext(browser()->GetProfile())
          ->GetDistilledPagePrefs();

  std::string result = content::EvalJs(contents, kGetFontSize).ExtractString();
  double oldFontSize;
  base::StringToDouble(result, &oldFontSize);

  // Set preference.
  const double kScale = 1.23;
  distilled_page_prefs->SetUserPrefTheme(mojom::Theme::kDark);
  distilled_page_prefs->SetFontFamily(mojom::FontFamily::kMonospace);
  distilled_page_prefs->SetUserPrefFontScaling(kScale);

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

// Flaky on MacOS (crbug.com/447462280).
#if BUILDFLAG(IS_MAC)
#define MAYBE_UISetsPrefs DISABLED_UISetsPrefs
#else
#define MAYBE_UISetsPrefs UISetsPrefs
#endif
IN_PROC_BROWSER_TEST_F(ReaderModeViewerSourceBrowserTest, MAYBE_UISetsPrefs) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Load the distilled page
  GURL view_url;
  expect_distillation_ = true;
  expect_distiller_page_ = true;
  GURL original_url("http://www.example.com/1");
  view_url = url_utils::GetDistillerViewUrlFromUrl(kWebDistillerScheme,
                                                   original_url, "Title");
  ViewSingleDistilledPage(view_url, "text/html");
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  // Wait for all currently executing scripts to finish. Otherwise, the
  // distiller object used to send the prefs to the browser from the JavaScript
  // may not exist, causing test flakiness.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(contents, kTestDistillerObject,
                           content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                           ISOLATED_WORLD_ID_CHROME_INTERNAL)
        .ExtractBool();
  }));

  DistilledPagePrefs* distilled_page_prefs =
      ReaderModeServiceFactory::GetForBrowserContext(browser()->GetProfile())
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

}  // namespace reader_mode
