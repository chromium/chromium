// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/rss_links_fetcher.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/mojom/rss_link_reader.mojom.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace feed {
namespace {

#if BUILDFLAG(IS_ANDROID)
class RssLinksFetcherTest : public AndroidBrowserTest {
#else
class RssLinksFetcherTest : public InProcessBrowserTest {
#endif
 public:
  RssLinksFetcherTest() = default;
  // AndroidBrowserTest:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(RssLinksFetcherTest, FetchSuccessfulFromHead) {
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  const GURL url =
      embedded_test_server()->GetURL("localhost", "/page_with_rss.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));

  base::HistogramTester histogram_tester;
  CallbackReceiver<std::vector<GURL>> rss_links;
  FetchRssLinks(url, web_contents, rss_links.Bind());
  std::vector<GURL> result = rss_links.RunAndGetResult();
  // Only valid RSS links in the head section should be returned.
  // Just check path on URLs relative to the local server, since its port
  // changes.
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("/rss.xml", result[0].path());
  EXPECT_EQ("/atom.xml", result[1].path());
  EXPECT_EQ(GURL("https://some/path.xml"), result[2]);

  ASSERT_TRUE(base::TimeTicks::IsHighResolution())
      << "The ContentSuggestions.Feed.WebFeed.GetRssLinksRendererTime "
         "histogram has microseconds precision and requires a high-resolution "
         "clock";
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectTotalCount(
      "ContentSuggestions.Feed.WebFeed.GetRssLinksRendererTime", 1);
}

IN_PROC_BROWSER_TEST_F(RssLinksFetcherTest, FetchSuccessfulFromBody) {
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  const GURL url = embedded_test_server()->GetURL(
      "localhost", "/page_with_rss_in_body.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));

  base::HistogramTester histogram_tester;
  CallbackReceiver<std::vector<GURL>> rss_links;
  FetchRssLinks(url, web_contents, rss_links.Bind());
  std::vector<GURL> result = rss_links.RunAndGetResult();
  // As there's no valid RSS links in the head, the ones from the body should be
  // returned.
  // Just check path on URLs relative to the local server, since its port
  // changes.
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("/rss-in-body.xml", result[0].path());
  EXPECT_EQ("/atom-in-body.xml", result[1].path());
  EXPECT_EQ(GURL("https://some/path-in-body.xml"), result[2]);

  ASSERT_TRUE(base::TimeTicks::IsHighResolution())
      << "The ContentSuggestions.Feed.WebFeed.GetRssLinksRendererTime "
         "histogram has microseconds precision and requires a high-resolution "
         "clock";
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectTotalCount(
      "ContentSuggestions.Feed.WebFeed.GetRssLinksRendererTime", 1);
}

}  // namespace
}  // namespace feed
