// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/browser/web_contents_main_frame_observer.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {
namespace {

const char* kSimpleArticlePath = "/dom_distiller/simple_article.html";
const char* kOriginalArticleTitle = "Test Page Title";

std::unique_ptr<content::WebContents> NewContentsWithSameParamsAs(
    content::WebContents* source_web_contents) {
  content::WebContents::CreateParams create_params(
      source_web_contents->GetBrowserContext());
  auto new_web_contents = content::WebContents::Create(create_params);
  DCHECK(new_web_contents);
  return new_web_contents;
}

// Helper class that blocks test execution until |observed_contents| enters a
// certain state. Subclasses specify the precise state by calling
// |new_url_loaded_runner_|.QuitClosure().Run() when |observed_contents| is
// ready.
class NavigationObserver : public content::WebContentsObserver {
 public:
  explicit NavigationObserver(content::WebContents* observed_contents) {
    content::WebContentsObserver::Observe(observed_contents);
  }

  void WaitUntilFinishedLoading() { new_url_loaded_runner_.Run(); }

 protected:
  base::RunLoop new_url_loaded_runner_;
};

class OriginalPageNavigationObserver : public NavigationObserver {
 public:
  using NavigationObserver::NavigationObserver;

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (!render_frame_host->GetParent())
      new_url_loaded_runner_.QuitClosure().Run();
  }
};

// DistilledPageObserver is used to detect if a distilled page has
// finished loading. This is done by checking how many times the title has
// been set rather than using "DidFinishLoad" directly due to the content
// being set by JavaScript.
class DistilledPageObserver : public NavigationObserver {
 public:
  explicit DistilledPageObserver(content::WebContents* observed_contents)
      : NavigationObserver(observed_contents),
        title_set_count_(0),
        loaded_distiller_page_(false) {}

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (!render_frame_host->GetParent() &&
        validated_url.scheme() == kDomDistillerScheme) {
      loaded_distiller_page_ = true;
      MaybeNotifyLoaded();
    }
  }

  void TitleWasSet(content::NavigationEntry* entry) override {
    // The title will be set twice on distilled pages; once for the placeholder
    // and once when the distillation has finished. Watch for the second time
    // as a signal that the JavaScript that sets the content has run.
    title_set_count_++;
    MaybeNotifyLoaded();
  }

 private:
  int title_set_count_;
  bool loaded_distiller_page_;

  // DidFinishLoad() can come after the two title settings.
  void MaybeNotifyLoaded() {
    if (title_set_count_ >= 2 && loaded_distiller_page_) {
      new_url_loaded_runner_.QuitClosure().Run();
    }
  }
};

class DomDistillerTabUtilsBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    if (!DistillerJavaScriptWorldIdIsSet()) {
      SetDistillerJavaScriptWorldId(content::ISOLATED_WORLD_ID_CONTENT_END);
    }
    ASSERT_TRUE(embedded_test_server()->Start());
    article_url_ = embedded_test_server()->GetURL(kSimpleArticlePath);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableDomDistiller);
  }

 protected:
  const GURL& article_url() const { return article_url_; }

  std::string GetPageTitle(content::WebContents* web_contents) const {
    return content::ExecuteScriptAndGetValue(web_contents->GetMainFrame(),
                                             "document.title")
        .GetString();
  }

 private:
  GURL article_url_;
};

IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsBrowserTest,
                       DistillCurrentPageSwapsWebContents) {
  content::WebContents* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // This blocks until the navigation has completely finished.
  ui_test_utils::NavigateToURL(browser(), article_url());

  DistillCurrentPageAndView(initial_web_contents);

  // Retrieve new web contents and wait for it to finish loading.
  content::WebContents* after_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(after_web_contents, nullptr);
  DistilledPageObserver(after_web_contents).WaitUntilFinishedLoading();

  // Verify the new URL is showing distilled content in a new WebContents.
  EXPECT_NE(initial_web_contents, after_web_contents);
  EXPECT_TRUE(
      after_web_contents->GetLastCommittedURL().SchemeIs(kDomDistillerScheme));
  EXPECT_EQ(kOriginalArticleTitle, GetPageTitle(after_web_contents));
}

IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsBrowserTest,
                       DistillAndViewCreatesNewWebContentsAndPreservesOld) {
  content::WebContents* source_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // This blocks until the navigation has completely finished.
  ui_test_utils::NavigateToURL(browser(), article_url());

  // Create destination WebContents and add it to the tab strip.
  browser()->tab_strip_model()->AppendWebContents(
      NewContentsWithSameParamsAs(source_web_contents),
      /* foreground = */ true);
  content::WebContents* destination_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  DistillAndView(source_web_contents, destination_web_contents);
  DistilledPageObserver(destination_web_contents).WaitUntilFinishedLoading();

  // Verify that the source WebContents is showing the original article.
  EXPECT_EQ(article_url(), source_web_contents->GetLastCommittedURL());
  EXPECT_EQ(kOriginalArticleTitle, GetPageTitle(source_web_contents));

  // Verify the destination WebContents is showing distilled content.
  EXPECT_TRUE(destination_web_contents->GetLastCommittedURL().SchemeIs(
      kDomDistillerScheme));
  EXPECT_EQ(kOriginalArticleTitle, GetPageTitle(destination_web_contents));

  content::WebContentsDestroyedWatcher destroyed_watcher(
      destination_web_contents);
  browser()->tab_strip_model()->CloseWebContentsAt(1, 0);
  destroyed_watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsBrowserTest, ToggleOriginalPage) {
  content::WebContents* source_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // This blocks until the navigation has completely finished.
  ui_test_utils::NavigateToURL(browser(), article_url());

  // Create and navigate to the distilled page.
  browser()->tab_strip_model()->AppendWebContents(
      NewContentsWithSameParamsAs(source_web_contents),
      /* foreground = */ true);
  content::WebContents* destination_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  DistillAndView(source_web_contents, destination_web_contents);
  DistilledPageObserver(destination_web_contents).WaitUntilFinishedLoading();
  ASSERT_TRUE(url_utils::IsDistilledPage(
      destination_web_contents->GetLastCommittedURL()));

  // Now return to the original page.
  ReturnToOriginalPage(destination_web_contents);
  OriginalPageNavigationObserver(destination_web_contents)
      .WaitUntilFinishedLoading();
  EXPECT_EQ(source_web_contents->GetLastCommittedURL(),
            destination_web_contents->GetLastCommittedURL());
}

}  // namespace
}  // namespace dom_distiller
