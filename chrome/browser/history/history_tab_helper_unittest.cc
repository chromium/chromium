// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_tab_helper.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/history_embeddings/history_embeddings_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/feed/feed_service_factory.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/test/stub_feed_api.h"
#endif  // BUILDFLAG(IS_ANDROID)

using testing::NiceMock;

namespace {

#if BUILDFLAG(IS_ANDROID)
class TestFeedApi : public feed::StubFeedApi {
 public:
  MOCK_METHOD1(WasUrlRecentlyNavigatedFromFeed, bool(const GURL&));
};
#endif  // BUILDFLAG(IS_ANDROID)

class MockHistoryClustersTabHelper : public HistoryClustersTabHelper {
 public:
  explicit MockHistoryClustersTabHelper(content::WebContents* web_contents)
      : HistoryClustersTabHelper(web_contents) {}
  ~MockHistoryClustersTabHelper() override = default;

  MOCK_METHOD(void,
              OnUpdatedHistoryForNavigation,
              (int64_t, base::Time, const GURL&),
              (override));
};

class MockHistoryEmbeddingsTabHelper : public HistoryEmbeddingsTabHelper {
 public:
  explicit MockHistoryEmbeddingsTabHelper(content::WebContents* web_contents)
      : HistoryEmbeddingsTabHelper(web_contents) {}
  ~MockHistoryEmbeddingsTabHelper() override = default;

  MOCK_METHOD(void,
              OnUpdatedHistoryForNavigation,
              (content::NavigationHandle*, base::Time, const GURL&),
              (override));
};

}  // namespace

class HistoryTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  HistoryTabHelperTest() = default;

  HistoryTabHelperTest(const HistoryTabHelperTest&) = delete;
  HistoryTabHelperTest& operator=(const HistoryTabHelperTest&) = delete;

  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if BUILDFLAG(IS_ANDROID)
    feed::FeedServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindLambdaForTesting([&](content::BrowserContext* context) {
          std::unique_ptr<KeyedService> result =
              feed::FeedService::CreateForTesting(&test_feed_api_);
          return result;
        }));
#endif  // BUILDFLAG(IS_ANDROID)
    history_service_ = HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::IMPLICIT_ACCESS);
    ASSERT_TRUE(history_service_);
    history_service_->AddPage(
        page_url_, base::Time::Now(), /*context_id=*/0,
        /*nav_entry_id=*/0,
        /*referrer=*/GURL(), history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
        history::SOURCE_BROWSED, history::VisitResponseCodeCategory::kNot404,
        /*did_replace_entry=*/false);
    HistoryTabHelper::CreateForWebContents(web_contents());
    HistoryTabHelper::FromWebContents(web_contents())
        ->SetForceEligibleTabForTesting(true);
  }

  void TearDown() override {
    // Drop unowned reference before destroying object that owns it.
    history_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory()}};
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
        url, base::BindLambdaForTesting([&](history::QueryURLResult result) {
          EXPECT_TRUE(result.success);
          title = base::UTF16ToUTF8(result.row.title());
          loop.Quit();
        }),
        &tracker_);
    loop.Run();
    return title;
  }

  base::TimeDelta QueryLastVisitDurationFromHistory(const GURL& url) {
    base::TimeDelta visit_duration;
    base::RunLoop loop;
    history_service_->QueryURLAndVisits(
        url, history::VisitQuery404sPolicy::kInclude404s,
        base::BindLambdaForTesting(
            [&](history::QueryURLAndVisitsResult result) {
              EXPECT_TRUE(result.success);
              if (!result.visits.empty()) {
                visit_duration = result.visits.back().visit_duration;
              }
              loop.Quit();
            }),
        &tracker_);
    loop.Run();
    return visit_duration;
  }

  history::MostVisitedURLList QueryMostVisitedURLs() {
    history::MostVisitedURLList result;
    std::string title;
    base::RunLoop loop;
    history_service_->QueryMostVisitedURLs(
        /*result_count=*/10,
        base::BindLambdaForTesting([&](history::MostVisitedURLList v) {
          result = v;
          loop.Quit();
        }),
        &tracker_);
    loop.Run();
    return result;
  }

  std::set<GURL> GetMostVisitedURLSet() {
    std::set<GURL> result;
    for (const history::MostVisitedURL& mv_url : QueryMostVisitedURLs()) {
      result.insert(mv_url.url);
    }
    return result;
  }

  const GURL page_url_ = GURL("http://foo.com");

 protected:
  base::CancelableTaskTracker tracker_;
  raw_ptr<history::HistoryService> history_service_;
#if BUILDFLAG(IS_ANDROID)
  TestFeedApi test_feed_api_;
#endif  // BUILDFLAG(IS_ANDROID)
};

class HistoryTabHelperVisitedFilteringTest
    : public HistoryTabHelperTest,
      public testing::WithParamInterface<bool> {
 public:
  HistoryTabHelperVisitedFilteringTest() {
    scoped_feature_list_.InitWithFeatureState(history::kVisitedLinksOn404,
                                              GetParam());
  }

  void SetUp() override {
    HistoryTabHelperTest::SetUp();

    web_contents()->SetUserData(
        HistoryClustersTabHelper::UserDataKey(),
        std::make_unique<NiceMock<MockHistoryClustersTabHelper>>(
            web_contents()));
    mock_history_clusters_tab_helper_ =
        static_cast<MockHistoryClustersTabHelper*>(
            HistoryClustersTabHelper::FromWebContents(web_contents()));

    web_contents()->SetUserData(
        HistoryEmbeddingsTabHelper::UserDataKey(),
        std::make_unique<NiceMock<MockHistoryEmbeddingsTabHelper>>(
            web_contents()));
    mock_history_embeddings_tab_helper_ =
        static_cast<MockHistoryEmbeddingsTabHelper*>(
            HistoryEmbeddingsTabHelper::FromWebContents(web_contents()));
  }

  void TearDown() override {
    mock_history_clusters_tab_helper_ = nullptr;
    mock_history_embeddings_tab_helper_ = nullptr;
    HistoryTabHelperTest::TearDown();
  }

 protected:
  raw_ptr<MockHistoryClustersTabHelper> mock_history_clusters_tab_helper_ =
      nullptr;
  raw_ptr<MockHistoryEmbeddingsTabHelper> mock_history_embeddings_tab_helper_ =
      nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(HistoryTabHelperVisitedFilteringTest, ShouldConsiderForNtpMostVisited) {
  bool are_404s_eligible_for_history =
      base::FeatureList::IsEnabled(history::kVisitedLinksOn404);
  NiceMock<content::MockNavigationHandle> navigation_handle(web_contents());
  const GURL some_url = GURL("https://someurl.com");
  navigation_handle.set_redirect_chain({some_url});

  // Simulate a user navigating to a forbidden resource.
  std::string raw_response_headers = "HTTP/1.1 403 Forbidden\r\n\r\n";
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      net::HttpResponseHeaders::TryToCreate(raw_response_headers);
  navigation_handle.set_response_headers(response_headers);

  // Create HistoryAddPageArgs for the 403 navigation.
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(some_url, base::Time(), 1,
                                                     &navigation_handle);

  // We should never be filtering out 403 navigations when determining NTP most
  // visited. This is because all error navigations other than 404 are eligible.
  EXPECT_EQ(args.consider_for_ntp_most_visited, true);

  // Simulate a user navigating to a resource that is not found.
  raw_response_headers = "HTTP/1.1 404 Not Found\r\n\r\n";
  response_headers =
      net::HttpResponseHeaders::TryToCreate(raw_response_headers);
  navigation_handle.set_response_headers(response_headers);

  // Create HistoryAddPageArgs for the 404 navigation.
  args = history_tab_helper()->CreateHistoryAddPageArgs(
      GURL("https://someurl.com"), base::Time(), 1, &navigation_handle);

  // If 404 error navigations are recorded in history, we should filter them out
  // when determining NTP most visited.
  EXPECT_EQ(args.consider_for_ntp_most_visited, !are_404s_eligible_for_history);
}

TEST_P(HistoryTabHelperVisitedFilteringTest, HistoryEmbeddingsHistoryClusters) {
  // Navigate to a URL that returns a 404 with a body.
  auto navigation_simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("http://someurl.com/custom404"), web_contents());
  navigation_simulator->Start();
  std::string raw_response_headers = "HTTP/1.1 404 Not Found\r\n\r\n";
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      net::HttpResponseHeaders::TryToCreate(raw_response_headers);
  navigation_simulator->SetResponseHeaders(response_headers);
  std::string response_body = "Not found, sorry";
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(response_body.size(), producer_handle,
                                 consumer_handle));
  navigation_simulator->SetResponseBody(std::move(consumer_handle));
  size_t actually_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle->WriteData(base::as_byte_span(response_body),
                                       MOJO_WRITE_DATA_FLAG_NONE,
                                       actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, response_body.size());

  // When calling HistoryTabHelper::DidFinishNavigation for a 404 navigation,
  // don't call HistoryClustersTabHelper::OnUpdatedHistoryForNavigation() or
  // HistoryEmbeddingsTabHelper::OnUpdatedHistoryForNavigation(). When
  // `history::kVisitedLinksOn404` is disabled, this happens because
  // `ShouldUpdateHistory()` will return false for 404s. When
  // `history::kVisitedLinksOn404` is enabled, `ShouldUpdateHistory()` will
  // return true for 404s, and we should explicitly skip notifying these tab
  // helpers for 404s, as 404 navigations aren't relevant for them.
  EXPECT_CALL(*mock_history_clusters_tab_helper_,
              OnUpdatedHistoryForNavigation(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_history_embeddings_tab_helper_,
              OnUpdatedHistoryForNavigation(testing::_, testing::_, testing::_))
      .Times(0);
  navigation_simulator->Commit();
}

INSTANTIATE_TEST_SUITE_P(All,
                         HistoryTabHelperVisitedFilteringTest,
                         ::testing::Bool());

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

TEST_F(HistoryTabHelperTest, ShouldUpdateVisitDurationInHistory) {
  const GURL url1("https://url1.com");
  const GURL url2("https://url2.com");

  web_contents_tester()->NavigateAndCommit(url1);
  // The duration shouldn't be set yet, since the visit is still open.
  EXPECT_TRUE(QueryLastVisitDurationFromHistory(url1).is_zero());

  // Once the user navigates on, the duration of the first visit should be
  // populated.
  web_contents_tester()->NavigateAndCommit(url2);
  EXPECT_FALSE(QueryLastVisitDurationFromHistory(url1).is_zero());
  EXPECT_TRUE(QueryLastVisitDurationFromHistory(url2).is_zero());

  // Closing the tab should finish the second visit and populate its duration.
  DeleteContents();
  EXPECT_FALSE(QueryLastVisitDurationFromHistory(url2).is_zero());
}

TEST_F(HistoryTabHelperTest, CreateAddPageArgsReferringURLMainFrameNoReferrer) {
  NiceMock<content::MockNavigationHandle> navigation_handle(web_contents());
  navigation_handle.set_redirect_chain({GURL("https://someurl.com")});
  navigation_handle.set_previous_primary_main_frame_url(
      GURL("http://previousurl.com"));
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(
          GURL("http://someurl.com"), base::Time(), 1, &navigation_handle);

  EXPECT_TRUE(args.referrer.is_empty());
}

TEST_F(HistoryTabHelperTest, CreateAddPageArgsHistoryTitleAfterPageReload) {
  NiceMock<content::MockNavigationHandle> navigation_handle(web_contents());
  navigation_handle.set_redirect_chain({GURL("https://someurl.com")});
  navigation_handle.set_previous_primary_main_frame_url(
      GURL("http://previousurl.com"));
  navigation_handle.set_reload_type(content::ReloadType::NORMAL);
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(
          GURL("http://someurl.com"), base::Time(), 1, &navigation_handle);

  EXPECT_EQ(args.title, web_contents()->GetTitle());
}

TEST_F(HistoryTabHelperTest,
       CreateAddPageArgsHistoryTitleAfterPageReloadBypassingCache) {
  NiceMock<content::MockNavigationHandle> navigation_handle(web_contents());
  navigation_handle.set_redirect_chain({GURL("https://someurl.com")});
  navigation_handle.set_previous_primary_main_frame_url(
      GURL("http://previousurl.com"));
  navigation_handle.set_reload_type(content::ReloadType::BYPASSING_CACHE);
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(
          GURL("http://someurl.com"), base::Time(), 1, &navigation_handle);

  EXPECT_EQ(args.title, web_contents()->GetTitle());
}

TEST_F(HistoryTabHelperTest,
       CreateAddPageArgsReferringURLMainFrameSameOriginReferrer) {
  NiceMock<content::MockNavigationHandle> navigation_handle(web_contents());
  navigation_handle.set_redirect_chain({GURL("https://someurl.com")});
  navigation_handle.set_previous_primary_main_frame_url(
      GURL("http://previousurl.com/abc"));
  auto referrer = blink::mojom::Referrer::New();
  referrer->url = navigation_handle.GetPreviousPrimaryMainFrameURL()
                      .DeprecatedGetOriginAsURL();
  referrer->policy = network::mojom::ReferrerPolicy::kDefault;
  navigation_handle.SetReferrer(std::move(referrer));
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(
          GURL("http://someurl.com"), base::Time(), 1, &navigation_handle);

  EXPECT_EQ(args.referrer, GURL("http://previousurl.com/abc"));
}

TEST_F(HistoryTabHelperTest,
       CreateAddPageArgsReferringURLMainFrameSameOriginReferrerDifferentPath) {
  NiceMock<content::MockNavigationHandle> navigation_handle(web_contents());
  navigation_handle.set_redirect_chain({GURL("https://someurl.com")});
  navigation_handle.set_previous_primary_main_frame_url(
      GURL("http://previousurl.com/def"));
  auto referrer = blink::mojom::Referrer::New();
  referrer->url = GURL("http://previousurl.com/abc");
  referrer->policy = network::mojom::ReferrerPolicy::kDefault;
  navigation_handle.SetReferrer(std::move(referrer));
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(
          GURL("http://someurl.com"), base::Time(), 1, &navigation_handle);

  EXPECT_EQ(args.referrer, GURL("http://previousurl.com/abc"));
}

TEST_F(HistoryTabHelperTest,
       CreateAddPageArgsReferringURLMainFrameCrossOriginReferrer) {
  NiceMock<content::MockNavigationHandle> navigation_handle(web_contents());
  auto referrer = blink::mojom::Referrer::New();
  referrer->url = GURL("http://crossorigin.com");
  referrer->policy = network::mojom::ReferrerPolicy::kDefault;
  navigation_handle.SetReferrer(std::move(referrer));
  navigation_handle.set_redirect_chain({GURL("https://someurl.com")});
  navigation_handle.set_previous_primary_main_frame_url(
      GURL("http://previousurl.com"));
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(
          GURL("http://someurl.com"), base::Time(), 1, &navigation_handle);

  EXPECT_EQ(args.referrer, GURL("http://crossorigin.com"));
}

TEST_F(HistoryTabHelperTest, CreateAddPageArgsReferringURLNotMainFrame) {
  content::RenderFrameHostTester* main_rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  main_rfh_tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = main_rfh_tester->AppendChild("subframe");
  NiceMock<content::MockNavigationHandle> navigation_handle(
      GURL("http://someurl.com"), subframe);
  navigation_handle.set_redirect_chain({GURL("https://someurl.com")});
  navigation_handle.set_previous_primary_main_frame_url(
      GURL("http://previousurl.com"));
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(
          GURL("http://someurl.com"), base::Time(), 1, &navigation_handle);

  // Should default to referrer if not in main frame and the referrer should not
  // be sent to the arbitrary previous URL that is set.
  EXPECT_NE(args.referrer, GURL("http://previousurl.com"));
}

TEST_F(HistoryTabHelperTest, CreateAddPageArgsFrameUrlWithValidInitiator) {
  // Create our initiator RenderFrameHost.
  content::RenderFrameHostTester* main_rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  main_rfh_tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = main_rfh_tester->AppendChild("subframe");
  const GURL initiator_url = GURL("http://previousurl.com");

  // Prepare a mock navigation from that initiator frame.
  const GURL test_url = GURL("http://testurl.com");
  NiceMock<content::MockNavigationHandle> navigation_handle(test_url, subframe);
  navigation_handle.set_initiator_origin(url::Origin::Create(initiator_url));
  // Simulate a navigation that is marked no-referrer (the value of which we
  // should ignore in favor of initiator origin).
  auto referrer = blink::mojom::Referrer::New();
  referrer->url = GURL();
  referrer->policy = network::mojom::ReferrerPolicy::kNever;
  navigation_handle.SetReferrer(std::move(referrer));
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(test_url, base::Time(), 1,
                                                     &navigation_handle);

  // `frame_url` should default to the last committed URL of the initiator
  // RenderFrameHost.
  ASSERT_TRUE(args.frame_url.has_value());
  EXPECT_EQ(args.frame_url.value(), initiator_url);
}

TEST_F(HistoryTabHelperTest, CreateAddPageArgsFrameUrlWithInvalidInitiator) {
  // Create our initiator RenderFrameHost but do not set the initiator origin.
  // This simulates an invalid or missing initiator frame.
  content::RenderFrameHostTester* main_rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  main_rfh_tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = main_rfh_tester->AppendChild("subframe");

  // Prepare a mock navigation from that initiator frame.
  const GURL test_url = GURL("http://testurl.com");
  NiceMock<content::MockNavigationHandle> navigation_handle(test_url, subframe);
  // Set a valid referrer with a default referrer policy.
  auto referrer = blink::mojom::Referrer::New();
  referrer->url = test_url;
  referrer->policy = network::mojom::ReferrerPolicy::kDefault;
  navigation_handle.SetReferrer(std::move(referrer));
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(test_url, base::Time(), 1,
                                                     &navigation_handle);

  // `frame_url` should fall back on referrer when we have an invalid initiator
  // origin.
  ASSERT_TRUE(args.frame_url.has_value());
  EXPECT_EQ(args.frame_url.value(), test_url);
}

TEST_F(HistoryTabHelperTest, CreateAddPageArgsHasOpenerWebContentsFirstPage) {
  std::unique_ptr<content::WebContents> opener_web_contents =
      CreateTestWebContents();
  content::WebContentsTester* opener_web_contents_tester =
      content::WebContentsTester::For(opener_web_contents.get());
  opener_web_contents_tester->NavigateAndCommit(
      GURL("https://opensnewtab.com/"));
  HistoryTabHelper::CreateForWebContents(opener_web_contents.get());
  HistoryTabHelper::FromWebContents(opener_web_contents.get())
      ->DidOpenRequestedURL(web_contents(), nullptr,
                            GURL("http://someurl.com/"), content::Referrer(),
                            WindowOpenDisposition::NEW_WINDOW,
                            ui::PAGE_TRANSITION_LINK, false, true);

  content::RenderFrameHostTester* main_rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  main_rfh_tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = main_rfh_tester->AppendChild("subframe");
  NiceMock<content::MockNavigationHandle> navigation_handle(
      GURL("http://someurl.com"), subframe);
  navigation_handle.set_redirect_chain({GURL("http://someurl.com")});
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(
          GURL("http://someurl.com"), base::Time(), 1, &navigation_handle);

  ASSERT_TRUE(args.opener.has_value());
  EXPECT_EQ(args.opener->url, GURL("https://opensnewtab.com/"));

  // When previous primary main frame is empty and our navigation type is LINK,
  // the top_level_url should be replaced by a valid opener URL.
  ASSERT_TRUE(args.top_level_url.has_value());
  EXPECT_EQ(args.top_level_url.value(), args.opener->url);
}

TEST_F(HistoryTabHelperTest, CreateAddPageArgsHasLiveOriginalOpenerChain) {
  // Prepare the original opener WebContents that will serve as the root of the
  // live original opener chain.
  std::unique_ptr<content::WebContents> live_original_opener =
      CreateTestWebContents();
  content::WebContentsTester* live_original_tester =
      content::WebContentsTester::For(live_original_opener.get());
  live_original_tester->NavigateAndCommit(GURL("https://opensnewtab.com/"));

  // The web_contents() for this test will have an empty opener property
  // but a valid live original opener chain. This mimics behavior such as
  // clicking on a link which opens in a new tab.
  content::WebContentsTester::For(web_contents())
      ->SetOriginalOpener(live_original_opener.get());

  // We want to create a HistoryTabHelper for the WebContents with an empty
  // opener, so the `top_level_url` is forced to be constructed with the live
  // original opener chain instead.
  HistoryTabHelper::CreateForWebContents(web_contents());
  HistoryTabHelper::FromWebContents(web_contents())
      ->DidOpenRequestedURL(web_contents(), nullptr,
                            GURL("http://someurl.com/"), content::Referrer(),
                            WindowOpenDisposition::NEW_WINDOW,
                            ui::PAGE_TRANSITION_LINK, false, true);

  // Preparing the NavigationHandle that HistoryTabHelper will use to construct
  // the HistoryAddPageArgs.
  content::RenderFrameHostTester* main_rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  main_rfh_tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = main_rfh_tester->AppendChild("subframe");
  NiceMock<content::MockNavigationHandle> navigation_handle(
      GURL("http://someurl.com"), subframe);
  navigation_handle.set_redirect_chain({GURL("http://someurl.com")});

  // Construct the HistoryAddPageArgs taking into consideration the WebContents
  // environment and NavigationHandle.
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(
          GURL("http://someurl.com"), base::Time(), 1, &navigation_handle);

  // When previous primary main frame and opener URLs are invalid, the
  // `top_level_url` should be populated with the live original opener URL.
  ASSERT_TRUE(args.top_level_url.has_value());
  EXPECT_EQ(args.top_level_url.value(), GURL("https://opensnewtab.com/"));
}

TEST_F(HistoryTabHelperTest, CreateAddPageArgsSameDocNavigationUsesOpener) {
  content::RenderFrameHostTester* main_rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  main_rfh_tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = main_rfh_tester->AppendChild("subframe");
  NiceMock<content::MockNavigationHandle> navigation_handle(
      GURL("http://someurl.com"), subframe);
  navigation_handle.set_redirect_chain({GURL("https://someurl.com")});
  navigation_handle.set_previous_primary_main_frame_url(
      GURL("http://previousurl.com"));
  navigation_handle.set_is_same_document(true);
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(
          GURL("http://someurl.com"), base::Time(), 1, &navigation_handle);

  ASSERT_TRUE(args.opener.has_value());
  EXPECT_EQ(args.opener->url, GURL("http://previousurl.com/"));
}

TEST_F(HistoryTabHelperTest,
       CreateAddPageArgsHasOpenerWebContentsNotFirstPage) {
  std::unique_ptr<content::WebContents> opener_web_contents =
      CreateTestWebContents();
  content::WebContentsTester* opener_web_contents_tester =
      content::WebContentsTester::For(opener_web_contents.get());
  opener_web_contents_tester->NavigateAndCommit(
      GURL("https://opensnewtab.com/"));

  HistoryTabHelper::CreateForWebContents(opener_web_contents.get());
  HistoryTabHelper::FromWebContents(opener_web_contents.get())
      ->DidOpenRequestedURL(web_contents(), nullptr,
                            GURL("http://someurl.com/"), content::Referrer(),
                            WindowOpenDisposition::NEW_WINDOW,
                            ui::PAGE_TRANSITION_LINK, false, true);

  content::RenderFrameHostTester* main_rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  main_rfh_tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = main_rfh_tester->AppendChild("subframe");
  NiceMock<content::MockNavigationHandle> navigation_handle(
      GURL("http://someurl.com/2"), subframe);
  navigation_handle.set_redirect_chain({GURL("http://someurl.com/2")});
  navigation_handle.set_previous_primary_main_frame_url(
      GURL("http://someurl.com"));
  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(
          GURL("http://someurl.com"), base::Time(), 1, &navigation_handle);

  EXPECT_FALSE(args.opener.has_value());

  // When there is a valid previous primary main frame, top-level url should
  // not be overwritten by an opener or live opener chain URL.
  ASSERT_TRUE(args.top_level_url.has_value());
  ASSERT_EQ(args.top_level_url.value(), GURL("http://someurl.com"));
}

TEST_F(HistoryTabHelperTest,
       CreateAddPageArgsPopulatesOnVisitContextAnnotations) {
  NiceMock<content::MockNavigationHandle> navigation_handle(web_contents());
  navigation_handle.set_redirect_chain({GURL("https://someurl.com")});

  std::string raw_response_headers = "HTTP/1.1 234 OK\r\n\r\n";
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      net::HttpResponseHeaders::TryToCreate(raw_response_headers);
  DCHECK(response_headers);
  navigation_handle.set_response_headers(response_headers);

  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(
          GURL("https://someurl.com"), base::Time(), 1, &navigation_handle);

  // Make sure the `context_annotations` are populated.
  ASSERT_TRUE(args.context_annotations.has_value());
  // Most of the actual fields can't be verified here, because the corresponding
  // data sources don't exist in this unit test (e.g. there's no Browser, no
  // other TabHelpers, etc). At least check the response code that was set up
  // above.
  EXPECT_EQ(args.context_annotations->response_code, 234);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(HistoryTabHelperTest, CreateAddPageArgsPopulatesAppId) {
  NiceMock<content::MockNavigationHandle> navigation_handle(web_contents());
  navigation_handle.set_redirect_chain({GURL("https://someurl.com")});

  std::string raw_response_headers = "HTTP/1.1 234 OK\r\n\r\n";
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      net::HttpResponseHeaders::TryToCreate(raw_response_headers);
  DCHECK(response_headers);
  navigation_handle.set_response_headers(response_headers);

  history_tab_helper()->SetAppId("org.chromium.testapp");

  history::HistoryAddPageArgs args =
      history_tab_helper()->CreateHistoryAddPageArgs(
          GURL("https://someurl.com"), base::Time(), 1, &navigation_handle);

  // Make sure the `app_id` is populated.
  ASSERT_EQ(*args.app_id, "org.chromium.testapp");
}

TEST_F(HistoryTabHelperTest, NonFeedNavigationsDoContributeToMostVisited) {
  GURL new_url("http://newurl.com");

  EXPECT_CALL(test_feed_api_, WasUrlRecentlyNavigatedFromFeed(new_url))
      .WillOnce(testing::Return(false));
  web_contents_tester()->NavigateAndCommit(new_url,
                                           ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  EXPECT_THAT(GetMostVisitedURLSet(), testing::Contains(new_url));
}

TEST_F(HistoryTabHelperTest, FeedNavigationsDoNotContributeToMostVisited) {
  GURL new_url("http://newurl.com");
  EXPECT_CALL(test_feed_api_, WasUrlRecentlyNavigatedFromFeed(new_url))
      .WillOnce(testing::Return(true));
  web_contents_tester()->NavigateAndCommit(new_url,
                                           ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  EXPECT_THAT(GetMostVisitedURLSet(), testing::Not(testing::Contains(new_url)));
}

#endif  // BUILDFLAG(IS_ANDROID)

enum class MPArchType {
  kFencedFrame,
  kPrerender,
};
class HistoryTabHelperMPArchTest
    : public HistoryTabHelperTest,
      public testing::WithParamInterface<MPArchType> {
 public:
  HistoryTabHelperMPArchTest() {
    switch (GetParam()) {
      case MPArchType::kFencedFrame:
        scoped_feature_list_.InitAndEnableFeatureWithParameters(
            blink::features::kFencedFrames,
            {{"implementation_type", "mparch"}});
        break;
      case MPArchType::kPrerender:
        scoped_feature_list_.InitAndDisableFeature(
            // Disable the memory requirement of Prerender2 so the test can run
            // on any bot.
            blink::features::kPrerender2MemoryControls);
        break;
    }
  }
  ~HistoryTabHelperMPArchTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HistoryTabHelperMPArchTest,
                         testing::Values(MPArchType::kFencedFrame,
                                         MPArchType::kPrerender));

TEST_P(HistoryTabHelperMPArchTest, DoNotAffectToLimitTitleUpdates) {
  content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());

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

  std::unique_ptr<content::NavigationSimulator> simulator;
  if (GetParam() == MPArchType::kFencedFrame) {
    // Navigate a fenced frame.
    GURL fenced_frame_url = GURL("https://fencedframe.com");
    content::RenderFrameHost* fenced_frame_root =
        content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
    simulator = content::NavigationSimulator::CreateRendererInitiated(
        fenced_frame_url, fenced_frame_root);
  } else if (GetParam() == MPArchType::kPrerender) {
    // Navigate a prerendering page.
    const GURL prerender_url = page_url_.Resolve("?prerendering");
    simulator = content::WebContentsTester::For(web_contents())
                    ->AddPrerenderAndStartNavigation(prerender_url);
  }
  ASSERT_NE(nullptr, simulator);
  simulator->Commit();

  // Further updates should be ignored.
  web_contents()->UpdateTitleForEntry(entry, u"title12");
  EXPECT_EQ("title10", QueryPageTitleFromHistory(page_url_));
}
