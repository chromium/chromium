// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/tab_utils.h"

#include <string.h>

#include <memory>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/dom_distiller/test_distillation_observers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/browser/test_distillability_observer.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace dom_distiller {
namespace {

const char* kSimpleArticlePath = "/dom_distiller/simple_article.html";
const char* kOriginalArticleTitle = "Test Page Title";
const char* kExpectedArticleHeading = "Test Page Title";
const char* kExpectedDocumentTitle = "Test Page Title";

std::unique_ptr<content::WebContents> NewContentsWithSameParamsAs(
    content::WebContents* source_web_contents) {
  content::WebContents::CreateParams create_params(
      source_web_contents->GetBrowserContext());
  auto new_web_contents = content::WebContents::Create(create_params);
  DCHECK(new_web_contents);
  return new_web_contents;
}

// FaviconUpdateWaiter waits for favicons to be changed after navigation.
// TODO(crbug.com/40123662): Combine with FaviconUpdateWaiter in
// chrome/browser/chrome_service_worker_browsertest.cc.
class FaviconUpdateWaiter : public favicon::FaviconDriverObserver {
 public:
  explicit FaviconUpdateWaiter(content::WebContents* web_contents) {
    scoped_observation_.Observe(
        favicon::ContentFaviconDriver::FromWebContents(web_contents));
  }

  FaviconUpdateWaiter(const FaviconUpdateWaiter&) = delete;
  FaviconUpdateWaiter& operator=(const FaviconUpdateWaiter&) = delete;

  ~FaviconUpdateWaiter() override = default;

  void Wait() {
    if (updated_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void StopObserving() { scoped_observation_.Reset(); }

 private:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override {
    updated_ = true;
    if (!quit_closure_.is_null())
      std::move(quit_closure_).Run();
  }

  bool updated_ = false;
  base::ScopedObservation<favicon::FaviconDriver,
                          favicon::FaviconDriverObserver>
      scoped_observation_{this};
  base::OnceClosure quit_closure_;
};

class DomDistillerTabUtilsBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    if (!DistillerJavaScriptWorldIdIsSet()) {
      SetDistillerJavaScriptWorldId(content::ISOLATED_WORLD_ID_CONTENT_END);
    }
    ASSERT_TRUE(https_server_->Start());
    article_url_ = https_server_->GetURL(kSimpleArticlePath);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableDomDistiller);
  }

 protected:
  DomDistillerTabUtilsBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }
  const GURL& article_url() const { return article_url_; }

  std::string GetDocumentTitle(content::WebContents* web_contents) const {
    return content::EvalJs(web_contents->GetPrimaryMainFrame(),
                           "document.title")
        .ExtractString();
  }

  std::string GetArticleHeading(content::WebContents* web_contents) const {
    return content::EvalJs(
               web_contents->GetPrimaryMainFrame(),
               "document.getElementById('title-holder').textContent")
        .ExtractString();
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;

 private:
  GURL article_url_;
};

// Disabled as flaky: https://crbug.com/1275025
IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsBrowserTest,
                       DISABLED_DistillCurrentPageSwapsWebContents) {
  content::WebContents* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TestDistillabilityObserver distillability_observer(initial_web_contents);
  DistillabilityResult expected_result;
  expected_result.is_distillable = true;
  expected_result.is_last = false;
  expected_result.is_mobile_friendly = false;

  // This blocks until the navigation has completely finished.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), article_url()));
  // This blocks until the page is found to be distillable.
  distillability_observer.WaitForResult(expected_result);

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
  EXPECT_EQ(kExpectedDocumentTitle, GetDocumentTitle(after_web_contents));
  EXPECT_EQ(kExpectedArticleHeading, GetArticleHeading(after_web_contents));
}

IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsBrowserTest,
                       BackForwardNavigationRegeneratesDistillabilitySignal) {
  content::WebContents* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TestDistillabilityObserver distillability_observer(initial_web_contents);
  DistillabilityResult article_result;
  article_result.is_distillable = true;
  article_result.is_last = true;
  article_result.is_mobile_friendly = false;

  // Navigate to URL 1.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), article_url()));
  distillability_observer.WaitForResult(article_result);

  GURL non_article_url =
      https_server_->GetURL("/dom_distiller/non_og_article.html");
  DistillabilityResult non_article_result;
  non_article_result.is_distillable = false;
  non_article_result.is_last = true;
  non_article_result.is_mobile_friendly = false;

  // Navigate to URL 2.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_article_url));
  distillability_observer.WaitForResult(non_article_result);

  // Navigate to the previous page.
  initial_web_contents->GetController().GoBack();
  distillability_observer.WaitForResult(article_result);
}

IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsBrowserTest,
                       DistillAndViewCreatesNewWebContentsAndPreservesOld) {
  content::WebContents* source_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // This blocks until the navigation has completely finished.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), article_url()));

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
  EXPECT_EQ(kOriginalArticleTitle, GetDocumentTitle(source_web_contents));

  // Verify the destination WebContents is showing distilled content.
  EXPECT_TRUE(destination_web_contents->GetLastCommittedURL().SchemeIs(
      kDomDistillerScheme));
  EXPECT_EQ(kExpectedDocumentTitle, GetDocumentTitle(destination_web_contents));
  EXPECT_EQ(kExpectedArticleHeading,
            GetArticleHeading(destination_web_contents));

  content::WebContentsDestroyedWatcher destroyed_watcher(
      destination_web_contents);
  browser()->tab_strip_model()->CloseWebContentsAt(1, 0);
  destroyed_watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsBrowserTest,
                       DomDistillDisableForBackForwardCache) {
  content::BackForwardCacheDisabledTester tester;

  GURL url1(article_url());
  content::WebContents* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url2(https_server_->GetURL("/title1.html"));

  TestDistillabilityObserver distillability_observer(initial_web_contents);
  DistillabilityResult expected_result;
  expected_result.is_distillable = true;
  expected_result.is_last = false;
  expected_result.is_mobile_friendly = false;

  // Navigate to the page
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  content::RenderFrameHost* main_frame = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  int process_id = main_frame->GetProcess()->GetID();
  int frame_routing_id = main_frame->GetRoutingID();
  distillability_observer.WaitForResult(expected_result);

  DistillCurrentPageAndView(initial_web_contents);

  // Navigate away while starting distillation. This should block bfcache.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
      process_id, frame_routing_id,
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::
              kDomDistiller_SelfDeletingRequestDelegate)));
}

IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsBrowserTest, SecurityStateIsNone) {
  content::WebContents* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TestDistillabilityObserver distillability_observer(initial_web_contents);
  DistillabilityResult expected_result;
  expected_result.is_distillable = true;
  expected_result.is_last = false;
  expected_result.is_mobile_friendly = false;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), article_url()));
  distillability_observer.WaitForResult(expected_result);

  // Check security state is not NONE.
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(initial_web_contents);
  ASSERT_NE(security_state::NONE, helper->GetSecurityLevel());

  DistillCurrentPageAndView(initial_web_contents);
  content::WebContents* after_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DistilledPageObserver(after_web_contents).WaitUntilFinishedLoading();

  // Now security state should be NONE.
  helper = SecurityStateTabHelper::FromWebContents(after_web_contents);
  ASSERT_EQ(security_state::NONE, helper->GetSecurityLevel());
}

// TODO(crbug.com/40776875): Flaky on Mac.
IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsBrowserTest,
                       FaviconFromOriginalPage) {
  content::WebContents* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  TestDistillabilityObserver distillability_observer(initial_web_contents);
  DistillabilityResult expected_result;
  expected_result.is_distillable = true;
  expected_result.is_last = false;
  expected_result.is_mobile_friendly = false;
  FaviconUpdateWaiter waiter(initial_web_contents);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), article_url()));
  // Ensure the favicon is loaded and the distillability result has also
  // loaded before proceeding with the test.
  waiter.Wait();
  distillability_observer.WaitForResult(expected_result);

  gfx::Image article_favicon = browser()->GetCurrentPageIcon();
  // Remove the FaviconUpdateWaiter because we are done with
  // initial_web_contents.
  waiter.StopObserving();

  DistillCurrentPageAndView(initial_web_contents);
  content::WebContents* after_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(after_web_contents, nullptr);
  DistilledPageObserver(after_web_contents).WaitUntilFinishedLoading();

  gfx::Image distilled_favicon = browser()->GetCurrentPageIcon();
  EXPECT_TRUE(gfx::test::AreImagesEqual(article_favicon, distilled_favicon));
}

class DomDistillerTabUtilsMPArchTest : public DomDistillerTabUtilsBrowserTest {
 public:
  content::WebContents* source_web_contents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  void NavigateAndDistill() {
    // Navigate to the initial page.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), article_url()));

    // Create destination WebContents and add it to the tab strip.
    browser()->tab_strip_model()->AppendWebContents(
        NewContentsWithSameParamsAs(source_web_contents()),
        /* foreground = */ true);
    content::WebContents* destination_web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(1);

    DistillAndView(source_web_contents(), destination_web_contents);
    DistilledPageObserver(destination_web_contents).WaitUntilFinishedLoading();
  }

  bool HasTaskTracker() {
    // SelfDeletingRequestDelegate uses a DeleteSoon. Make sure the
    // DeleteSoon task runs before checking TaskTracker.
    base::RunLoop().RunUntilIdle();
    return DomDistillerServiceFactory::GetForBrowserContext(
               source_web_contents()->GetBrowserContext())
        ->HasTaskTrackerForTesting(article_url());
  }
};

IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsMPArchTest,
                       TaskTrackerRemovedWhenPrimaryPageChanged) {
  NavigateAndDistill();
  // Ensure the TaskTracker for distilling the source article exist.
  EXPECT_TRUE(HasTaskTracker());

  // Activate the source web contents.
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Navigate away from the source page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_->GetURL("/title1.html")));

  // SelfDeletingRequestDelegate deletes itself when PrimaryPageChanged() is
  // called. Ensure that the TaskTracker has been removed.
  EXPECT_FALSE(HasTaskTracker());
}

class DomDistillerTabUtilsFencedFrameTest
    : public DomDistillerTabUtilsMPArchTest {
 public:
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsFencedFrameTest,
                       TaskTrackerNotRemovedByFencedFrame) {
  NavigateAndDistill();
  // Ensure the TaskTracker for distilling the source article exist.
  EXPECT_TRUE(HasTaskTracker());

  // Activate the source web contents.
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Add a fenced frame into the source web contents.
  const GURL fenced_frame_url =
      https_server_->GetURL("/fenced_frames/title1.html");
  ASSERT_TRUE(fenced_frame_test_helper().CreateFencedFrame(
      source_web_contents()->GetPrimaryMainFrame(), fenced_frame_url));

  // Ensure that the navigation in the fenced frame doesn't affect the
  // SelfDeletingRequestDelegate.
  EXPECT_TRUE(HasTaskTracker());
}

class DomDistillerTabUtilsPrerenderTest
    : public DomDistillerTabUtilsMPArchTest {
 public:
  DomDistillerTabUtilsPrerenderTest()
      : prerender_helper_(base::BindRepeating(
            &DomDistillerTabUtilsMPArchTest::source_web_contents,
            base::Unretained(this))) {}

  void SetUpOnMainThread() override {
    prerender_helper_.RegisterServerRequestMonitor(https_server_.get());
    DomDistillerTabUtilsBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsPrerenderTest,
                       TaskTrackerNotRemovedByPrerendering) {
  NavigateAndDistill();
  // Ensure the TaskTracker for distilling the source article exist.
  EXPECT_TRUE(HasTaskTracker());

  // Activate the source web contents.
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Add a prerender.
  const GURL prerender_url = https_server_->GetURL("/title1.html");
  content::FrameTreeNodeId host_id =
      prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver prerender_observer(
      *source_web_contents(), host_id);
  EXPECT_FALSE(prerender_observer.was_activated());

  // Ensure that prerendering doesn't affect the SelfDeletingRequestDelegate.
  EXPECT_TRUE(HasTaskTracker());

  // Activate the prerendered page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(prerender_observer.was_activated());

  // SelfDeletingRequestDelegate deletes itself when PrimaryPageChanged() is
  // called. Ensure that the TaskTracker has been removed.
  EXPECT_FALSE(HasTaskTracker());
}

}  // namespace

}  // namespace dom_distiller
