// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/page_content_annotations/page_stability_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_content_annotations/content/browser/page_settled_monitor.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {

class PageSettledMonitorChromeBrowserTest
    : public PageStabilityBrowserTestBase {
 public:
  PageSettledMonitorChromeBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageSettledMonitor,
        {// Effectively disable the timeout to prevent flakes.
         {features::kPageStabilityTimeout.name, "30000ms"},
         {features::kObservationDelayTimeout.name, "30000ms"}});
  }
  ~PageSettledMonitorChromeBrowserTest() override = default;

  void LoadPage() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetPageStabilityTestURL()));

    base::test::TestFuture<bool> future;
    web_contents()->GetPrimaryMainFrame()->InsertVisualStateCallback(
        future.GetCallback());
    ASSERT_TRUE(future.Wait()) << "Timeout waiting for syncing with renderer";
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageSettledMonitorChromeBrowserTest,
                       WaitOnNetworkFetch) {
  LoadPage();

  PageSettledMonitor monitor(main_frame(),
                             std::make_unique<PageSettledMonitor::Delegate>(
                                 PageSettledMonitor::PageStabilityConfig{}));

  ASSERT_EQ(GetOutputText(), "INITIAL");
  InitiateNetworkRequest();

  base::test::TestFuture<void> result;
  monitor.Wait(web_contents(), result.GetCallback());

  // Wait long enough to have some confidence the monitor is blocking on the
  // network request.
  Sleep(base::Seconds(1));

  // The fetch hasn't resolved yet, the monitor should still be waiting on
  // network fetches to resolve.
  ASSERT_EQ(GetOutputText(), "INITIAL");
  EXPECT_FALSE(result.IsReady());

  // Complete the fetch, ensure the monitor completes.
  Respond("NETWORK DONE");
  ASSERT_TRUE(result.Wait());
  ASSERT_EQ(GetOutputText(), "NETWORK DONE");
}

IN_PROC_BROWSER_TEST_F(PageSettledMonitorChromeBrowserTest, WaitOnMainThread) {
  LoadPage();

  ASSERT_EQ(GetOutputText(), "INITIAL");

  PageSettledMonitor monitor(main_frame(),
                             std::make_unique<PageSettledMonitor::Delegate>(
                                 PageSettledMonitor::PageStabilityConfig{}));

  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "window.doBusyWork(/*tasks_to_run=*/4, /*task_duration_ms=*/400)"));

  base::test::TestFuture<void> result;
  monitor.Wait(web_contents(), result.GetCallback());

  // Wait long enough to have some confidence the monitor is blocking on the
  // main thread.
  Sleep(base::Seconds(1));
  EXPECT_FALSE(result.IsReady());

  // But it should eventually resolve once the tasks finish.
  ASSERT_TRUE(result.Wait());
  EXPECT_EQ(GetOutputText(), "WORK DONE");
}

IN_PROC_BROWSER_TEST_F(PageSettledMonitorChromeBrowserTest,
                       NoRenderPageStability_NotWaitOnNetworkFetch) {
  LoadPage();

  ASSERT_EQ(GetOutputText(), "INITIAL");

  PageSettledMonitor monitor(main_frame(),
                             std::make_unique<PageSettledMonitor::Delegate>(
                                 /*page_stability_config=*/std::nullopt));

  InitiateNetworkRequest();

  base::test::TestFuture<void> result;
  monitor.Wait(web_contents(), result.GetCallback());

  ASSERT_TRUE(result.Wait());
  EXPECT_EQ(GetOutputText(), "INITIAL");
}

IN_PROC_BROWSER_TEST_F(PageSettledMonitorChromeBrowserTest,
                       NoRenderPageStability_NotWaitOnMainThread) {
  LoadPage();

  ASSERT_EQ(GetOutputText(), "INITIAL");

  PageSettledMonitor monitor(main_frame(),
                             std::make_unique<PageSettledMonitor::Delegate>(
                                 /*page_stability_config=*/std::nullopt));

  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "window.doBusyWork(/*tasks_to_run=*/10000)"));

  base::test::TestFuture<void> result;
  monitor.Wait(web_contents(), result.GetCallback());

  ASSERT_TRUE(result.Wait());
  EXPECT_EQ(GetOutputText(), "INITIAL");
}

}  // namespace

}  // namespace page_content_annotations
