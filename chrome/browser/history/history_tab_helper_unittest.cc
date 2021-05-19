// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_tab_helper.h"

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/feed/v2/feed_service_factory.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/test/stub_feed_api.h"
#endif

namespace {

#if defined(OS_ANDROID)
class TestFeedApi : public feed::StubFeedApi {
 public:
  MOCK_METHOD1(WasUrlRecentlyNavigatedFromFeed, bool(const GURL&));
};
#endif

class HistoryTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  HistoryTabHelperTest() {}

  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if defined(OS_ANDROID)
    feed::FeedServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindLambdaForTesting([&](content::BrowserContext* context) {
          std::unique_ptr<KeyedService> result =
              feed::FeedService::CreateForTesting(&test_feed_api_);
          return result;
        }));
#endif
    ASSERT_TRUE(profile()->CreateHistoryService());
    history_service_ = HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::IMPLICIT_ACCESS);
    ASSERT_TRUE(history_service_);
    history_service_->AddPage(
        page_url_, base::Time::Now(), /*context_id=*/nullptr,
        /*nav_entry_id=*/0,
        /*referrer=*/GURL(), history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
        history::SOURCE_BROWSED, /*did_replace_entry=*/false,
        /*floc_allowed=*/true);
    HistoryTabHelper::CreateForWebContents(web_contents());
  }

  HistoryTabHelper* history_tab_helper() {
    return HistoryTabHelper::FromWebContents(web_contents());
  }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  std::string QueryPageTitleFromHistory(const GURL& url) {
    std::string title;
    base::RunLoop loop;
    history_service_->QueryURL(
        url, /*want_visits=*/false,
        base::BindLambdaForTesting([&](history::QueryURLResult result) {
          EXPECT_TRUE(result.success);
          title = base::UTF16ToUTF8(result.row.title());
          loop.Quit();
        }),
        &tracker_);
    loop.Run();
    return title;
  }

  history::MostVisitedURLList QueryMostVisitedURLs() {
    history::MostVisitedURLList result;
    std::string title;
    base::RunLoop loop;
    history_service_->QueryMostVisitedURLs(
        /*result_count=*/10, /*days_back=*/1,
        base::BindLambdaForTesting([&](history::MostVisitedURLList v) {
          result = v;
          loop.Quit();
        }),
        &tracker_);
    loop.Run();
    return result;
  }

  std::set<GURL> GetMostVisistedURLSet() {
    std::set<GURL> result;
    for (const history::MostVisitedURL& mv_url : QueryMostVisitedURLs()) {
      result.insert(mv_url.url);
    }
    return result;
  }

  const GURL page_url_ = GURL("http://foo.com");

 protected:
  base::CancelableTaskTracker tracker_;
  history::HistoryService* history_service_;
#if defined(OS_ANDROID)
  TestFeedApi test_feed_api_;
#endif

  DISALLOW_COPY_AND_ASSIGN(HistoryTabHelperTest);
};

TEST_F(HistoryTabHelperTest, ShouldUpdateTitleInHistory) {
  web_contents_tester()->NavigateAndCommit(page_url_);

  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_NE(nullptr, entry);

  web_contents()->UpdateTitleForEntry(entry, u"title1");
  EXPECT_EQ("title1", QueryPageTitleFromHistory(page_url_));
}

TEST_F(HistoryTabHelperTest, ShouldLimitTitleUpdatesPerPage) {
  web_contents_tester()->NavigateAndCommit(page_url_);

  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_NE(nullptr, entry);

  // The first 10 title updates are accepted and update history, as per
  // history::kMaxTitleChanges.
  for (int i = 1; i <= history::kMaxTitleChanges; ++i) {
    const std::string title = base::StringPrintf("title%d", i);
    web_contents()->UpdateTitleForEntry(entry, base::UTF8ToUTF16(title));
  }

  ASSERT_EQ("title10", QueryPageTitleFromHistory(page_url_));

  // Further updates should be ignored.
  web_contents()->UpdateTitleForEntry(entry, u"title11");
  EXPECT_EQ("title10", QueryPageTitleFromHistory(page_url_));
}

#if defined(OS_ANDROID)

TEST_F(HistoryTabHelperTest, NonFeedNavigationsDoContributeToMostVisited) {
  GURL new_url("http://newurl.com");

  EXPECT_CALL(test_feed_api_, WasUrlRecentlyNavigatedFromFeed(new_url))
      .WillOnce(testing::Return(false));
  web_contents_tester()->NavigateAndCommit(new_url,
                                           ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  EXPECT_THAT(GetMostVisistedURLSet(), testing::Contains(new_url));
}

TEST_F(HistoryTabHelperTest, FeedNavigationsDoNotContributeToMostVisited) {
  GURL new_url("http://newurl.com");
  EXPECT_CALL(test_feed_api_, WasUrlRecentlyNavigatedFromFeed(new_url))
      .WillOnce(testing::Return(true));
  web_contents_tester()->NavigateAndCommit(new_url,
                                           ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  EXPECT_THAT(GetMostVisistedURLSet(),
              testing::Not(testing::Contains(new_url)));
}

#endif

}  // namespace
