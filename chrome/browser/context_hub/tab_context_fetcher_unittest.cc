// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/tab_context_fetcher.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "components/page_content_annotations/content/mock_page_content_services.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;

class MockPageContentExtractionService
    : public page_content_annotations::MockPageContentExtractionService {
 public:
  MockPageContentExtractionService() = default;
  ~MockPageContentExtractionService() override = default;

  MOCK_METHOD(void,
              GetExtractedPageContentAndEligibilityForPageAsync,
              (content::Page&,
               page_content_annotations::PageContentExtractionService::
                   GetExtractedPageContentAndEligibilityCallback,
               bool),
              (override));
};

class TabContextFetcherTest : public ChromeRenderViewHostTestHarness {
 public:
  TabContextFetcherTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TabContextFetcherTest() override = default;

 protected:
  std::unique_ptr<content::WebContents> CreateTab(
      const GURL& url = GURL("https://example.com")) {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    sessions::SessionTabHelper::CreateForWebContents(
        web_contents.get(), sessions::SessionTabHelper::DelegateLookup());
    content::WebContentsTester::For(web_contents.get())->NavigateAndCommit(url);
    return web_contents;
  }

  void MockPageContentExtraction(const std::string& title = "Page Title") {
    auto apc = base::MakeRefCounted<
        page_content_annotations::RefCountedAnnotatedPageContent>();
    apc->data.mutable_main_frame_data()->set_title(title);
    page_content_annotations::ExtractedPageContentResult extracted_result(
        std::move(apc), base::Time::Now(),
        /*is_eligible_for_server_upload=*/true,
        /*screenshot_data=*/{});
    EXPECT_CALL(mock_page_content_extraction_service_,
                GetExtractedPageContentAndEligibilityForPageAsync(_, _, true))
        .WillOnce(RunOnceCallback<1>(std::move(extracted_result)))
        .RetiresOnSaturation();
  }

  MockPageContentExtractionService mock_page_content_extraction_service_;
};

TEST_F(TabContextFetcherTest, TabAlreadyLoadedExtractsImmediately) {
  auto web_contents = CreateTab(GURL("https://example.com/loaded"));
  MockPageContentExtraction("Loaded Title");

  base::test::TestFuture<TabContextFetcher::TabContextResult> future;
  TabContextFetcher fetcher(mock_page_content_extraction_service_,
                            web_contents.get(), future.GetCallback());
  fetcher.Start();

  EXPECT_TRUE(future.IsReady());
  auto [tab, page_context] = future.Take();
  ASSERT_TRUE(tab.has_value());
  EXPECT_EQ(tab->url, GURL("https://example.com/loaded"));
  ASSERT_TRUE(page_context.has_value());
  EXPECT_EQ(page_context->annotated_page_content().main_frame_data().title(),
            "Loaded Title");
}

TEST_F(TabContextFetcherTest, DiscardedTabLoadsBeforeExtraction) {
  auto web_contents = CreateTab(GURL("https://example.com/discarded"));
  web_contents->SetWasDiscarded(true);
  web_contents->GetController().SetNeedsReload();

  EXPECT_CALL(mock_page_content_extraction_service_,
              GetExtractedPageContentAndEligibilityForPageAsync(_, _, true))
      .Times(0);

  base::test::TestFuture<TabContextFetcher::TabContextResult> future;
  TabContextFetcher fetcher(mock_page_content_extraction_service_,
                            web_contents.get(), future.GetCallback());
  fetcher.Start();

  EXPECT_TRUE(web_contents->IsLoading());
  EXPECT_FALSE(future.IsReady());

  testing::Mock::VerifyAndClearExpectations(
      &mock_page_content_extraction_service_);
  MockPageContentExtraction("Reloaded Title");

  auto simulator = content::NavigationSimulator::CreateFromPending(
      web_contents->GetController());
  simulator->Commit();

  EXPECT_TRUE(future.IsReady());
  auto [tab, page_context] = future.Take();
  ASSERT_TRUE(tab.has_value());
  EXPECT_EQ(tab->url, GURL("https://example.com/discarded"));
  ASSERT_TRUE(page_context.has_value());
  EXPECT_EQ(page_context->annotated_page_content().main_frame_data().title(),
            "Reloaded Title");
}

TEST_F(TabContextFetcherTest,
       CommittedTabWithSubresourceLoadingExtractsImmediately) {
  auto web_contents = CreateTab(GURL("https://example.com/loaded"));
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(web_contents->GetPrimaryMainFrame())
          ->AppendChild("subframe");
  auto subframe_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.com/subframe"), subframe);
  subframe_simulator->Start();
  EXPECT_TRUE(web_contents->IsLoading());
  EXPECT_FALSE(web_contents->HasUncommittedNavigationInPrimaryMainFrame());
  MockPageContentExtraction("Loaded Title");

  base::test::TestFuture<TabContextFetcher::TabContextResult> future;
  TabContextFetcher fetcher(mock_page_content_extraction_service_,
                            web_contents.get(), future.GetCallback());
  fetcher.Start();

  EXPECT_TRUE(future.IsReady());
  auto [tab, page_context] = future.Take();
  ASSERT_TRUE(tab.has_value());
  EXPECT_EQ(tab->url, GURL("https://example.com/loaded"));
  ASSERT_TRUE(page_context.has_value());
  EXPECT_EQ(page_context->annotated_page_content().main_frame_data().title(),
            "Loaded Title");
}

TEST_F(TabContextFetcherTest, NavigatingTabWaitsForCommitBeforeExtraction) {
  auto web_contents = CreateTab(GURL("https://example.com/loading"));
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/loading_next"), web_contents.get());
  simulator->Start();
  EXPECT_TRUE(web_contents->IsLoading());

  EXPECT_CALL(mock_page_content_extraction_service_,
              GetExtractedPageContentAndEligibilityForPageAsync(_, _, true))
      .Times(0);

  base::test::TestFuture<TabContextFetcher::TabContextResult> future;
  TabContextFetcher fetcher(mock_page_content_extraction_service_,
                            web_contents.get(), future.GetCallback());
  fetcher.Start();

  EXPECT_FALSE(future.IsReady());

  testing::Mock::VerifyAndClearExpectations(
      &mock_page_content_extraction_service_);
  MockPageContentExtraction("Loaded Next Title");

  simulator->Commit();

  EXPECT_TRUE(future.IsReady());
  auto [tab, page_context] = future.Take();
  ASSERT_TRUE(tab.has_value());
  EXPECT_EQ(tab->url, GURL("https://example.com/loading_next"));
  ASSERT_TRUE(page_context.has_value());
  EXPECT_EQ(page_context->annotated_page_content().main_frame_data().title(),
            "Loaded Next Title");
}

TEST_F(TabContextFetcherTest, TabLoadTimeoutFinishesWithoutPageContext) {
  auto web_contents = CreateTab(GURL("https://example.com/slow"));
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/slow_next"), web_contents.get());
  simulator->Start();
  EXPECT_TRUE(web_contents->IsLoading());

  EXPECT_CALL(mock_page_content_extraction_service_,
              GetExtractedPageContentAndEligibilityForPageAsync(_, _, true))
      .Times(0);

  base::test::TestFuture<TabContextFetcher::TabContextResult> future;
  TabContextFetcher fetcher(mock_page_content_extraction_service_,
                            web_contents.get(), future.GetCallback());
  fetcher.Start();

  EXPECT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(features::kTabLoadTimeout.Get());

  EXPECT_TRUE(future.IsReady());
  auto [tab, page_context] = future.Take();
  ASSERT_TRUE(tab.has_value());
  EXPECT_EQ(tab->url, GURL("https://example.com/slow"));
  EXPECT_FALSE(page_context.has_value());
}

TEST_F(TabContextFetcherTest,
       UncommittedNavigationDoesNotTriggerExtractionEarly) {
  auto web_contents = CreateTab(GURL("https://example.com/initial"));
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/aborted"), web_contents.get());
  simulator->Start();

  EXPECT_CALL(mock_page_content_extraction_service_,
              GetExtractedPageContentAndEligibilityForPageAsync(_, _, true))
      .Times(0);

  base::test::TestFuture<TabContextFetcher::TabContextResult> future;
  TabContextFetcher fetcher(mock_page_content_extraction_service_,
                            web_contents.get(), future.GetCallback());
  fetcher.Start();
  EXPECT_FALSE(future.IsReady());

  // Fail the navigation without committing.
  simulator->Fail(net::ERR_ABORTED);
  EXPECT_FALSE(future.IsReady());

  // Fast forward past the timeout to verify fallback extraction occurs.
  MockPageContentExtraction("Initial Page Title");
  task_environment()->FastForwardBy(features::kTabLoadTimeout.Get());

  EXPECT_TRUE(future.IsReady());
  auto [tab, page_context] = future.Take();
  ASSERT_TRUE(tab.has_value());
  EXPECT_EQ(tab->url, GURL("https://example.com/initial"));
  ASSERT_TRUE(page_context.has_value());
  EXPECT_EQ(page_context->annotated_page_content().main_frame_data().title(),
            "Initial Page Title");
}

TEST_F(TabContextFetcherTest, TabDestroyedWhileLoading) {
  auto web_contents = CreateTab(GURL("https://example.com/destroyed"));
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/destroyed_next"), web_contents.get());
  simulator->Start();
  EXPECT_TRUE(web_contents->IsLoading());

  base::test::TestFuture<TabContextFetcher::TabContextResult> future;
  auto fetcher = std::make_unique<TabContextFetcher>(
      mock_page_content_extraction_service_, web_contents.get(),
      future.GetCallback());
  fetcher->Start();

  EXPECT_FALSE(future.IsReady());

  simulator.reset();
  web_contents.reset();

  EXPECT_TRUE(future.IsReady());
  auto [tab, page_context] = future.Take();
  EXPECT_FALSE(tab.has_value());
  EXPECT_FALSE(page_context.has_value());
}

TEST_F(TabContextFetcherTest, NullWebContentsAtStart) {
  base::test::TestFuture<TabContextFetcher::TabContextResult> future;
  TabContextFetcher fetcher(mock_page_content_extraction_service_, nullptr,
                            future.GetCallback());
  fetcher.Start();

  EXPECT_TRUE(future.IsReady());
  auto [tab, page_context] = future.Take();
  EXPECT_FALSE(tab.has_value());
  EXPECT_FALSE(page_context.has_value());
}

TEST_F(TabContextFetcherTest, StartCalledMultipleTimesIsIdempotent) {
  auto web_contents = CreateTab(GURL("https://example.com/loading"));
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/loading_next"), web_contents.get());
  simulator->Start();
  EXPECT_TRUE(web_contents->IsLoading());

  base::test::TestFuture<TabContextFetcher::TabContextResult> future;
  TabContextFetcher fetcher(mock_page_content_extraction_service_,
                            web_contents.get(), future.GetCallback());
  fetcher.Start();
  EXPECT_FALSE(future.IsReady());

  // Fast forward partially into the timeout window.
  base::TimeDelta partial_delay = features::kTabLoadTimeout.Get() / 2;
  task_environment()->FastForwardBy(partial_delay);

  // Calling Start() again while loading should be a no-op and must not restart
  // the timer.
  fetcher.Start();

  EXPECT_CALL(mock_page_content_extraction_service_,
              GetExtractedPageContentAndEligibilityForPageAsync(_, _, true))
      .Times(0);

  // Advance by the remaining time. If the timer were restarted by the second
  // Start(), the timeout would not trigger here. Because Start() was a no-op,
  // the original timer fires on time.
  task_environment()->FastForwardBy(features::kTabLoadTimeout.Get() -
                                    partial_delay);

  EXPECT_TRUE(future.IsReady());
  auto [tab, page_context] = future.Take();
  ASSERT_TRUE(tab.has_value());
  EXPECT_EQ(tab->url, GURL("https://example.com/loading"));
  EXPECT_FALSE(page_context.has_value());
}

}  // namespace
}  // namespace context_hub
