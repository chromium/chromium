// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
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
#include "components/dom_distiller/core/dom_distiller_store.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/dom_distiller_test_util.h"
#include "components/dom_distiller/core/fake_distiller.h"
#include "components/dom_distiller/core/fake_distiller_page.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace dom_distiller {

using leveldb_proto::test::FakeDB;
using test::FakeDistiller;
using test::MockDistillerPage;
using test::MockDistillerFactory;
using test::MockDistillerPageFactory;
using test::util::CreateStoreWithFakeDB;
using testing::HasSubstr;
using testing::Not;

namespace {

const char kGetLoadIndicatorClassName[] =
    "window.domAutomationController.send("
        "document.getElementById('loadingIndicator').className)";

const char kGetContent[] =
    "window.domAutomationController.send("
        "document.getElementById('content').innerHTML)";

const char kGetTitle[] =
    "window.domAutomationController.send("
        "document.title)";

const char kGetBodyClass[] =
    "window.domAutomationController.send("
        "document.body.className)";

const char kGetFontSize[] =
    "window.domAutomationController.send("
        "window.getComputedStyle(document.documentElement).fontSize)";

const unsigned kDarkToolbarThemeColor = 0xFF1A1A1A;

const char kTestDistillerObject[] =
    "window.domAutomationController.send("
    "typeof distiller == 'object')";

void AddEntry(const ArticleEntry& e, FakeDB<ArticleEntry>::EntryMap* map) {
  (*map)[e.entry_id()] = e;
}

ArticleEntry CreateEntry(const std::string& entry_id,
                         const std::string& page_url) {
  ArticleEntry entry;
  entry.set_entry_id(entry_id);
  if (!page_url.empty()) {
    ArticleEntryPage* page = entry.add_pages();
    page->set_url(page_url);
  }
  return entry;
}

}  // namespace

class DomDistillerViewerSourceBrowserTest : public InProcessBrowserTest {
 public:
  DomDistillerViewerSourceBrowserTest() {}
  ~DomDistillerViewerSourceBrowserTest() override {}

  void SetUpOnMainThread() override {
    if (!DistillerJavaScriptWorldIdIsSet()) {
      SetDistillerJavaScriptWorldId(content::ISOLATED_WORLD_ID_CONTENT_END);
    }
    database_model_ = new FakeDB<ArticleEntry>::EntryMap;
  }

  void TearDownOnMainThread() override { delete database_model_; }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableDomDistiller);
  }

  static std::unique_ptr<KeyedService> Build(content::BrowserContext* context) {
    FakeDB<ArticleEntry>* fake_db = new FakeDB<ArticleEntry>(database_model_);
    distiller_factory_ = new MockDistillerFactory();
    MockDistillerPageFactory* distiller_page_factory_ =
        new MockDistillerPageFactory();
    std::unique_ptr<DomDistillerContextKeyedService> service(
        new DomDistillerContextKeyedService(
            std::unique_ptr<DomDistillerStoreInterface>(CreateStoreWithFakeDB(
                fake_db, FakeDB<ArticleEntry>::EntryMap())),
            std::unique_ptr<DistillerFactory>(distiller_factory_),
            std::unique_ptr<DistillerPageFactory>(distiller_page_factory_),
            std::unique_ptr<DistilledPagePrefs>(new DistilledPagePrefs(
                Profile::FromBrowserContext(context)->GetPrefs()))));
    fake_db->InitCallback(true);
    fake_db->LoadCallback(true);
    if (expect_distillation_) {
      // There will only be destillation of an article if the database contains
      // the article.
      FakeDistiller* distiller = new FakeDistiller(true);
      EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
          .WillOnce(testing::Return(distiller));
    }
    if (expect_distiller_page_) {
      MockDistillerPage* distiller_page = new MockDistillerPage();
      EXPECT_CALL(*distiller_page_factory_, CreateDistillerPageImpl())
          .WillOnce(testing::Return(distiller_page));
    }
    return std::move(service);
  }

  void ViewSingleDistilledPage(const GURL& url,
                               const std::string& expected_mime_type);
  void PrefTest(bool is_error_page);
  // Database entries.
  static FakeDB<ArticleEntry>::EntryMap* database_model_;
  static bool expect_distillation_;
  static bool expect_distiller_page_;
  static MockDistillerFactory* distiller_factory_;
};

FakeDB<ArticleEntry>::EntryMap*
    DomDistillerViewerSourceBrowserTest::database_model_;
bool DomDistillerViewerSourceBrowserTest::expect_distillation_ = false;
bool DomDistillerViewerSourceBrowserTest::expect_distiller_page_ = false;
MockDistillerFactory* DomDistillerViewerSourceBrowserTest::distiller_factory_ =
    NULL;

// The DomDistillerViewerSource renders untrusted content, so ensure no bindings
// are enabled when the article exists in the database.
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       NoWebUIBindingsArticleExists) {
  // Ensure there is one item in the database, which will trigger distillation.
  const ArticleEntry entry = CreateEntry("DISTILLED", "http://example.com/1");
  AddEntry(entry, database_model_);
  expect_distillation_ = true;
  expect_distiller_page_ = true;
  const GURL url = url_utils::GetDistillerViewUrlFromEntryId(
      kDomDistillerScheme, entry.entry_id());
  ViewSingleDistilledPage(url, "text/html");
}

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
// Flaky on Linux: see http://crbug.com/606040.
#if defined(OS_LINUX)
#define MAYBE_NoWebUIBindingsViewUrl DISABLED_NoWebUIBindingsViewUrl
#else
#define MAYBE_NoWebUIBindingsViewUrl NoWebUIBindingsViewUrl
#endif
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       MAYBE_NoWebUIBindingsViewUrl) {
  // We should expect distillation for any valid URL.
  expect_distillation_ = true;
  expect_distiller_page_ = true;
  GURL view_url("http://www.example.com/1");
  const GURL url =
      url_utils::GetDistillerViewUrlFromUrl(kDomDistillerScheme, view_url);
  ViewSingleDistilledPage(url, "text/html");
}

void DomDistillerViewerSourceBrowserTest::ViewSingleDistilledPage(
    const GURL& url,
    const std::string& expected_mime_type) {
  // Ensure the correct factory is used for the DomDistillerService.
  dom_distiller::DomDistillerServiceFactory::GetInstance()
      ->SetTestingFactoryAndUse(browser()->profile(),
                                base::BindRepeating(&Build));

  // Navigate to a URL which the source should respond to.
  ui_test_utils::NavigateToURL(browser(), url);

  // Ensure no bindings for the loaded |url|.
  content::WebContents* contents_after_nav =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents_after_nav != NULL);
  EXPECT_EQ(url, contents_after_nav->GetLastCommittedURL());
  const content::RenderFrameHost* render_frame_host =
      contents_after_nav->GetMainFrame();
  EXPECT_EQ(0, render_frame_host->GetEnabledBindings());
  EXPECT_EQ(expected_mime_type, contents_after_nav->GetContentsMimeType());
}

#if defined(OS_LINUX)
// Flaky on Ubuntu-12.04 bots: https://crbug.com/604362
#define MAYBE_TestBadUrlErrorPage DISABLED_TestBadUrlErrorPage
#else
#define MAYBE_TestBadUrlErrorPage TestBadUrlErrorPage
#endif
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       MAYBE_TestBadUrlErrorPage) {
  GURL url("chrome-distiller://bad");

  // Navigate to a distiller URL.
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait for the page load to complete as the first page completes the root
  // document.
  content::WaitForLoadStop(contents);

  ASSERT_TRUE(contents != NULL);
  EXPECT_EQ(url, contents->GetLastCommittedURL());

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetContent, &result));
  EXPECT_THAT(result, HasSubstr(l10n_util::GetStringUTF8(
      IDS_DOM_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_CONTENT)));
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetTitle, &result));
  EXPECT_THAT(result, HasSubstr(l10n_util::GetStringUTF8(
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


#if defined(OS_LINUX)
// Flaky on Ubuntu-12.04 bots: https://crbug.com/604362
#define MAYBE_EmptyURLShouldNotCrash DISABLED_EmptyURLShouldNotCrash
#else
#define MAYBE_EmptyURLShouldNotCrash EmptyURLShouldNotCrash
#endif
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       MAYBE_EmptyURLShouldNotCrash) {
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
                       EarlyTemplateLoad) {
  dom_distiller::DomDistillerServiceFactory::GetInstance()
      ->SetTestingFactoryAndUse(browser()->profile(),
                                base::BindRepeating(&Build));

  scoped_refptr<content::MessageLoopRunner> distillation_done_runner =
      new content::MessageLoopRunner;

  FakeDistiller* distiller = new FakeDistiller(
      false,
      distillation_done_runner->QuitClosure());
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(testing::Return(distiller));

  // Navigate to a URL.
  GURL url(dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, GURL("http://urlthatlooksvalid.com")));
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  Navigate(&params);
  distillation_done_runner->Run();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait for the page load to complete (should only be template).
  content::WaitForLoadStop(contents);
  std::string result;
  // Loading spinner should be on screen at this point.
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetLoadIndicatorClassName , &result));
  EXPECT_EQ("visible", result);

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetContent , &result));
  EXPECT_THAT(result, Not(HasSubstr("content")));

  // Finish distillation and make sure the spinner has been replaced by text.
  std::vector<scoped_refptr<ArticleDistillationUpdate::RefCountedPageProto> >
      update_pages;
  std::unique_ptr<DistilledArticleProto> article(new DistilledArticleProto());

  scoped_refptr<base::RefCountedData<DistilledPageProto> > page_proto =
      new base::RefCountedData<DistilledPageProto>();
  page_proto->data.set_url("http://foo.html");
  page_proto->data.set_html("<div>content</div>");
  update_pages.push_back(page_proto);
  *(article->add_pages()) = page_proto->data;

  ArticleDistillationUpdate update(update_pages, true, false);
  distiller->RunDistillerUpdateCallback(update);

  content::WaitForLoadStop(contents);

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetContent , &result));
  EXPECT_THAT(result, HasSubstr("content"));
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       DistillerJavaScriptExposed) {
  // Navigate to a distiller URL.
  GURL url(std::string(kDomDistillerScheme) + "://url");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  Navigate(&params);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait for the page load to complete (this will be a distiller error page).
  content::WaitForLoadStop(contents);

  // Execute in isolated world; where all distiller scripts are run.
  EXPECT_EQ(true, content::EvalJsWithManualReply(
                      contents, kTestDistillerObject,
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
  content::WaitForLoadStop(contents);

  bool result;
  // Execute in main world, the distiller object should not be here.
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents, kTestDistillerObject, &result));
  EXPECT_FALSE(result);
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
  content::WaitForLoadStop(contents);

  bool result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents, kTestDistillerObject, &result));
  EXPECT_FALSE(result);
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest, MultiPageArticle) {
  expect_distillation_ = false;
  expect_distiller_page_ = true;
  dom_distiller::DomDistillerServiceFactory::GetInstance()
      ->SetTestingFactoryAndUse(browser()->profile(),
                                base::BindRepeating(&Build));

  scoped_refptr<content::MessageLoopRunner> distillation_done_runner =
      new content::MessageLoopRunner;

  FakeDistiller* distiller = new FakeDistiller(
      false,
      distillation_done_runner->QuitClosure());
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(testing::Return(distiller));

  // Setup observer to inspect the RenderViewHost after committed navigation.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a URL and wait for the distiller to flush contents to the page.
  GURL url(dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, GURL("http://urlthatlooksvalid.com")));
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  Navigate(&params);
  distillation_done_runner->Run();

  // Fake a multi-page response from distiller.

  std::vector<scoped_refptr<ArticleDistillationUpdate::RefCountedPageProto> >
      update_pages;
  std::unique_ptr<DistilledArticleProto> article(new DistilledArticleProto());

  // Flush page 1.
  {
    scoped_refptr<base::RefCountedData<DistilledPageProto> > page_proto =
        new base::RefCountedData<DistilledPageProto>();
    page_proto->data.set_url("http://foobar.1.html");
    page_proto->data.set_html("<div>Page 1 content</div>");
    update_pages.push_back(page_proto);
    *(article->add_pages()) = page_proto->data;

    ArticleDistillationUpdate update(update_pages, true, false);
    distiller->RunDistillerUpdateCallback(update);

    // Wait for the page load to complete as the first page completes the root
    // document.
    content::WaitForLoadStop(contents);

    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, kGetLoadIndicatorClassName , &result));
    EXPECT_EQ("visible", result);

    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, kGetContent , &result));
    EXPECT_THAT(result, HasSubstr("Page 1 content"));
    EXPECT_THAT(result, Not(HasSubstr("Page 2 content")));
  }

  // Flush page 2.
  {
    scoped_refptr<base::RefCountedData<DistilledPageProto> > page_proto =
        new base::RefCountedData<DistilledPageProto>();
    page_proto->data.set_url("http://foobar.2.html");
    page_proto->data.set_html("<div>Page 2 content</div>");
    update_pages.push_back(page_proto);
    *(article->add_pages()) = page_proto->data;

    ArticleDistillationUpdate update(update_pages, false, false);
    distiller->RunDistillerUpdateCallback(update);

    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, kGetLoadIndicatorClassName , &result));
    EXPECT_EQ("hidden", result);

    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, kGetContent , &result));
    EXPECT_THAT(result, HasSubstr("Page 1 content"));
    EXPECT_THAT(result, HasSubstr("Page 2 content"));
  }

  // Complete the load.
  distiller->RunDistillerCallback(std::move(article));
  base::RunLoop().RunUntilIdle();

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetLoadIndicatorClassName, &result));
  EXPECT_EQ("hidden", result);
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetContent , &result));
  EXPECT_THAT(result, HasSubstr("Page 1 content"));
  EXPECT_THAT(result, HasSubstr("Page 2 content"));
}

// Flaky on Ubuntu-12.04 bots: https://crbug.com/606037
#if defined(OS_LINUX)
#define MAYBE_PrefChange DISABLED_PrefChange
#else
#define MAYBE_PrefChange PrefChange
#endif
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       MAYBE_PrefChange) {
  PrefTest(false);
}

#if defined(OS_LINUX)
// Flaky on Ubuntu-12.04 bots: https://crbug.com/604362
#define MAYBE_PrefChangeError DISABLED_PrefChangeError
#else
#define MAYBE_PrefChangeError PrefChangeError
#endif
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       MAYBE_PrefChangeError) {
  PrefTest(true);
}


void DomDistillerViewerSourceBrowserTest::PrefTest(bool is_error_page) {
  GURL url;
  if (is_error_page) {
    expect_distillation_ = false;
    expect_distiller_page_ = false;
    url = GURL("chrome-distiller://bad");
  } else {
    expect_distillation_ = true;
    expect_distiller_page_ = true;
    GURL view_url("http://www.example.com/1");
    url = url_utils::GetDistillerViewUrlFromUrl(kDomDistillerScheme, view_url);
  }
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ViewSingleDistilledPage(url, "text/html");
  content::WaitForLoadStop(contents);
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetBodyClass, &result));
  EXPECT_EQ("light sans-serif", result);

  DistilledPagePrefs* distilled_page_prefs =
       DomDistillerServiceFactory::GetForBrowserContext(
            browser()->profile())->GetDistilledPagePrefs();

  // Test theme.
  distilled_page_prefs->SetTheme(DistilledPagePrefs::DARK);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetBodyClass, &result));
  EXPECT_EQ("dark sans-serif", result);

  // Verify that the theme color for the tab is updated as well.
  EXPECT_EQ(kDarkToolbarThemeColor, contents->GetThemeColor());

  // Test font family.
  distilled_page_prefs->SetFontFamily(DistilledPagePrefs::SERIF);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      content::ExecuteScriptAndExtractString(contents, kGetBodyClass, &result));
  EXPECT_EQ("dark serif", result);

  // Test font scaling.
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetFontSize, &result));
  double oldFontSize;
  base::StringToDouble(result, &oldFontSize);

  const double kScale = 1.23;
  distilled_page_prefs->SetFontScaling(kScale);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetFontSize, &result));
  double fontSize;
  base::StringToDouble(result, &fontSize);
  ASSERT_FLOAT_EQ(kScale, fontSize/oldFontSize);
}

IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest, PrefPersist) {
  expect_distillation_ = false;
  expect_distiller_page_ = false;
  const GURL url("chrome-distiller://bad");
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(contents);

  std::string result;
  DistilledPagePrefs* distilled_page_prefs =
       DomDistillerServiceFactory::GetForBrowserContext(
            browser()->profile())->GetDistilledPagePrefs();

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetFontSize, &result));
  double oldFontSize;
  base::StringToDouble(result, &oldFontSize);

  // Set preference.
  const double kScale = 1.23;
  distilled_page_prefs->SetTheme(DistilledPagePrefs::DARK);
  distilled_page_prefs->SetFontFamily(DistilledPagePrefs::SERIF);
  distilled_page_prefs->SetFontScaling(kScale);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetBodyClass, &result));
  EXPECT_EQ("dark serif", result);
  EXPECT_EQ(kDarkToolbarThemeColor, contents->GetThemeColor());
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetFontSize, &result));
  double fontSize;
  base::StringToDouble(result, &fontSize);
  ASSERT_FLOAT_EQ(kScale, fontSize/oldFontSize);

  // Make sure perf persist across web pages.
  GURL url2("chrome-distiller://bad2");
  ui_test_utils::NavigateToURL(browser(), url2);
  content::WaitForLoadStop(contents);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetBodyClass, &result));
  EXPECT_EQ("dark serif", result);
  EXPECT_EQ(kDarkToolbarThemeColor, contents->GetThemeColor());

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetFontSize, &result));
  base::StringToDouble(result, &fontSize);
  ASSERT_FLOAT_EQ(kScale, fontSize/oldFontSize);
}

}  // namespace dom_distiller
