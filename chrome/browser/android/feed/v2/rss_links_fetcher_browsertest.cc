// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/v2/rss_links_fetcher.h"

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

IN_PROC_BROWSER_TEST_F(RssLinksFetcherTest, FetchSuccessful) {
  auto* tab = TabAndroid::FromWebContents(
      chrome_test_utils::GetActiveWebContents(this));
  const GURL url =
      embedded_test_server()->GetURL("localhost", "/page_with_rss.html");
  ASSERT_TRUE(content::NavigateToURL(tab->web_contents(), url));

  CallbackReceiver<WebFeedPageInformation> page_info;
  FetchRssLinks(url, tab, page_info.Bind());
  WebFeedPageInformation result = page_info.RunAndGetResult();
  // Just check path on URLs relative to the local server, since it's port
  // changes.
  EXPECT_EQ("/page_with_rss.html", result.url().path());
  ASSERT_EQ(3u, result.GetRssUrls().size());
  EXPECT_EQ("/rss.xml", result.GetRssUrls()[0].path());
  EXPECT_EQ("/atom.xml", result.GetRssUrls()[1].path());
  EXPECT_EQ(GURL("https://some/path.xml"), result.GetRssUrls()[2]);
}

}  // namespace
}  // namespace feed
