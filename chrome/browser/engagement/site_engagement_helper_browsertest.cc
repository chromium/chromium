// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_engagement/content/site_engagement_helper.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/site_engagement/content/engagement_type.h"
#include "components/site_engagement/content/site_engagement_metrics.h"
#include "components/site_engagement/content/site_engagement_observer.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace site_engagement {

class TestOneShotTimer : public base::OneShotTimer {
 public:
  TestOneShotTimer() = default;
  ~TestOneShotTimer() override = default;

  // base::OneShotTimer:
  void Start(const base::Location& posted_from,
             base::TimeDelta delay,
             base::OnceClosure user_task) override {
    base::OneShotTimer::Start(posted_from, base::Seconds(0),
                              std::move(user_task));

    // Updates |restarted_| if the timer is restarted.
    if (started_)
      restarted_ = true;

    started_ = true;
  }

  bool restarted() { return restarted_; }

 private:
  bool started_ = false;
  // Used to know if SiteEngagementService::Helper::DidFinishNavigation()
  // handles a page in the prerendering.
  bool restarted_ = false;
};

class SiteEngagementHelperBrowserTest : public InProcessBrowserTest {
 public:
  SiteEngagementHelperBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&SiteEngagementHelperBrowserTest::web_contents,
                                base::Unretained(this))) {}
  ~SiteEngagementHelperBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());
  }

  // Set a pause timer on the input tracker for test purposes.
  void SetInputTrackerPauseTimer(SiteEngagementService::Helper* helper) {
    input_tracker_timer_ = new TestOneShotTimer;
    helper->input_tracker_.SetPauseTimerForTesting(
        base::WrapUnique(input_tracker_timer_.get()));
  }

  // Set a pause timer on the media tracker for test purposes.
  void SetMediaTrackerPauseTimer(SiteEngagementService::Helper* helper) {
    media_tracker_timer_ = new TestOneShotTimer;
    helper->media_tracker_.SetPauseTimerForTesting(
        base::WrapUnique(media_tracker_timer_.get()));
  }

  bool IsInputTrackerTimerRestarted(SiteEngagementService::Helper* helper) {
    return input_tracker_timer_->restarted();
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  base::HistogramTester histogram_tester_;
  raw_ptr<TestOneShotTimer, AcrossTasksDanglingUntriaged> input_tracker_timer_;
  raw_ptr<TestOneShotTimer, AcrossTasksDanglingUntriaged> media_tracker_timer_;
};

// Tests if SiteEngagementHelper checks the primary main frame in the
// prerendering.
IN_PROC_BROWSER_TEST_F(SiteEngagementHelperBrowserTest,
                       SiteEngagementHelperInPrerendering) {
  SiteEngagementService::Helper* helper =
      SiteEngagementService::Helper::FromWebContents(web_contents());
  SetInputTrackerPauseTimer(helper);

  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // SiteEngagementMetrics::kEngagementTypeHistogram is 2 with
  // kFirstDailyEngagement and kNavigation.
  histogram_tester()->ExpectTotalCount(
      SiteEngagementMetrics::kEngagementTypeHistogram, 2);
  // It's the first navigation after overriding the timer.
  EXPECT_FALSE(IsInputTrackerTimerRestarted(helper));

  // Loads a page in the prerender.
  auto prerender_url = embedded_test_server()->GetURL("/simple.html");
  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  // SiteEngagementMetrics::kEngagementTypeHistogram is not updated with the
  // prerendering.
  histogram_tester()->ExpectTotalCount(
      SiteEngagementMetrics::kEngagementTypeHistogram, 2);
  // Should not be restarted since
  // SiteEngagementService::Helper::DidFinishNavigation() skips the page in the
  // prerendering.
  EXPECT_FALSE(IsInputTrackerTimerRestarted(helper));

  prerender_helper()->NavigatePrimaryPage(*web_contents(), prerender_url);

  // Makes sure that the page is activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());
  // Should be restarted since the page is activated from the prerendering.
  EXPECT_TRUE(IsInputTrackerTimerRestarted(helper));
  // Renderer initiated activation is not counted as an engagement event. As a
  // result, SiteEngagementMetrics::kEngagementTypeHistogram maintains a value
  // of 2 with the prerendering activation.
  //
  // TODO(crbug.com/40164098): Add a test for browser-initiated/omnibox
  // navigations when available.
  histogram_tester()->ExpectTotalCount(
      SiteEngagementMetrics::kEngagementTypeHistogram, 2);
  // SiteEngagementMetrics::kEngagementTypeHistogram should be 1 with
  // EngagementType::kNavigation.
  histogram_tester()->ExpectBucketCount(
      SiteEngagementMetrics::kEngagementTypeHistogram,
      EngagementType::kNavigation, 1);
}

class ObserverTester : public SiteEngagementObserver {
 public:
  explicit ObserverTester(SiteEngagementService* service)
      : SiteEngagementObserver(service) {}

  void OnEngagementEvent(content::WebContents* web_contents,
                         const GURL& url,
                         double score,
                         double old_score,
                         EngagementType type,
                         const std::optional<webapps::AppId>& app_id) override {
    last_updated_type_ = type;
    last_updated_url_ = url;
    if (type == type_waiting_) {
      if (quit_closure_)
        std::move(quit_closure_).Run();
    }
  }

  void WaitForEngagementEvent(EngagementType type) {
    type_waiting_ = type;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  EngagementType last_updated_type() { return last_updated_type_; }
  const GURL& last_updated_url() { return last_updated_url_; }

 private:
  base::OnceClosure quit_closure_;
  GURL last_updated_url_;
  EngagementType last_updated_type_ = EngagementType::kLast;
  EngagementType type_waiting_ = EngagementType::kLast;
};

IN_PROC_BROWSER_TEST_F(SiteEngagementHelperBrowserTest,
                       SiteEngagementHelperMediaTrackerInPrerendering) {
  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(browser()->profile());
  ObserverTester tester(service);

  SiteEngagementService::Helper* helper =
      SiteEngagementService::Helper::FromWebContents(web_contents());
  SetMediaTrackerPauseTimer(helper);

  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(tester.last_updated_type(), EngagementType::kNavigation);
  EXPECT_EQ(tester.last_updated_url(), url);

  // Load a page in the prerender.
  GURL prerender_url =
      embedded_test_server()->GetURL("/media/unified_autoplay.html");
  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  content::RenderFrameHost* prerendered_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);
  // Since the prerendered page couldn't have a user gesture, it runs JS with
  // EXECUTE_SCRIPT_NO_USER_GESTURE. Requesting playing video without a user
  // gesture results in the promise rejected.
  EXPECT_EQ(false, content::EvalJs(
                       prerendered_frame_host, "attemptPlay();",
                       content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_EQ(tester.last_updated_type(), EngagementType::kNavigation);
  EXPECT_EQ(tester.last_updated_url(), url);

  // Navigate the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // The page should be activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());

  EXPECT_TRUE(
      content::EvalJs(web_contents()->GetPrimaryMainFrame(), "attemptPlay();")
          .ExtractBool());

  tester.WaitForEngagementEvent(EngagementType::kMediaVisible);
  EXPECT_EQ(tester.last_updated_type(), EngagementType::kMediaVisible);
  EXPECT_EQ(tester.last_updated_url(), prerender_url);
}

}  // namespace site_engagement
