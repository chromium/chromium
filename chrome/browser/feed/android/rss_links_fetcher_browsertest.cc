// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/android/rss_links_fetcher.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/feed_feature_list.h"
#include "components/feed/mojom/rss_link_reader.mojom.h"
#include "components/sync_sessions/session_sync_service_impl.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

class RssLinksFetcherTest : public AndroidBrowserTest {
 public:
  RssLinksFetcherTest() { features_.InitAndEnableFeature(kWebFeed); }
  // AndroidBrowserTest:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(RssLinksFetcherTest, FetchSuccessfulFromHead) {
  auto* tab = TabAndroid::FromWebContents(
      chrome_test_utils::GetActiveWebContents(this));
  const GURL url =
      embedded_test_server()->GetURL("localhost", "/page_with_rss.html");
  ASSERT_TRUE(content::NavigateToURL(tab->web_contents(), url));

  CallbackReceiver<std::vector<GURL>> rss_links;
  FetchRssLinks(url, tab, rss_links.Bind());
  std::vector<GURL> result = rss_links.RunAndGetResult();
  // Only valid RSS links in the head section should be returned.
  // Just check path on URLs relative to the local server, since its port
  // changes.
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("/rss.xml", result[0].path());
  EXPECT_EQ("/atom.xml", result[1].path());
  EXPECT_EQ(GURL("https://some/path.xml"), result[2]);
}

IN_PROC_BROWSER_TEST_F(RssLinksFetcherTest, FetchSuccessfulFromBody) {
  auto* tab = TabAndroid::FromWebContents(
      chrome_test_utils::GetActiveWebContents(this));
  const GURL url = embedded_test_server()->GetURL(
      "localhost", "/page_with_rss_in_body.html");
  ASSERT_TRUE(content::NavigateToURL(tab->web_contents(), url));

  CallbackReceiver<std::vector<GURL>> rss_links;
  FetchRssLinks(url, tab, rss_links.Bind());
  std::vector<GURL> result = rss_links.RunAndGetResult();
  // As there's no valid RSS links in the head, the ones from the body should be
  // returned.
  // Just check path on URLs relative to the local server, since its port
  // changes.
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("/rss-in-body.xml", result[0].path());
  EXPECT_EQ("/atom-in-body.xml", result[1].path());
  EXPECT_EQ(GURL("https://some/path-in-body.xml"), result[2]);
}

}  // namespace
}  // namespace feed
