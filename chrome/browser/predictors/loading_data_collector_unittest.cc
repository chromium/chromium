// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_data_collector.h"

#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Key;
using testing::StrictMock;

namespace predictors {

namespace {

NavigationId GetNextId() {
  static NavigationId::Generator generator;
  return generator.GenerateNextId();
}

}  // namespace

class LoadingDataCollectorTest : public testing::Test {
 public:
  LoadingDataCollectorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        profile_(std::make_unique<TestingProfile>()) {
    LoadingPredictorConfig config;
    PopulateTestConfig(&config);
    mock_predictor_ =
        std::make_unique<StrictMock<MockResourcePrefetchPredictor>>(
            config, profile_.get()),
    collector_ = std::make_unique<LoadingDataCollector>(mock_predictor_.get(),
                                                        nullptr, config);
  }

  void SetUp() override {
    LoadingDataCollector::SetAllowPortInUrlsForTesting(false);
    content::RunAllTasksUntilIdle();  // Runs the DB lookup.
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<StrictMock<MockResourcePrefetchPredictor>> mock_predictor_;
  std::unique_ptr<LoadingDataCollector> collector_;
};

TEST_F(LoadingDataCollectorTest, HandledResourceTypes) {
  EXPECT_TRUE(PageRequestSummary::IsHandledResourceType(
      network::mojom::RequestDestination::kStyle, "bogus/mime-type"));
  EXPECT_TRUE(PageRequestSummary::IsHandledResourceType(
      network::mojom::RequestDestination::kStyle, ""));
  EXPECT_FALSE(PageRequestSummary::IsHandledResourceType(
      network::mojom::RequestDestination::kWorker, "text/css"));
  EXPECT_FALSE(PageRequestSummary::IsHandledResourceType(
      network::mojom::RequestDestination::kWorker, ""));
  EXPECT_TRUE(PageRequestSummary::IsHandledResourceType(
      network::mojom::RequestDestination::kEmpty, "text/css"));
  EXPECT_FALSE(PageRequestSummary::IsHandledResourceType(
      network::mojom::RequestDestination::kEmpty, "bogus/mime-type"));
  EXPECT_FALSE(PageRequestSummary::IsHandledResourceType(
      network::mojom::RequestDestination::kEmpty, ""));
  EXPECT_TRUE(PageRequestSummary::IsHandledResourceType(
      network::mojom::RequestDestination::kEmpty, "application/font-woff"));
  EXPECT_TRUE(PageRequestSummary::IsHandledResourceType(
      network::mojom::RequestDestination::kEmpty, "font/woff2"));
  EXPECT_TRUE(PageRequestSummary::IsHandledResourceType(
      network::mojom::RequestDestination::kEmpty, "application/javascript"));
  EXPECT_TRUE(PageRequestSummary::IsHandledResourceType(
      network::mojom::RequestDestination::kDocument, "text/html"));
  EXPECT_TRUE(PageRequestSummary::IsHandledResourceType(
      network::mojom::RequestDestination::kDocument, ""));
}

TEST_F(LoadingDataCollectorTest, ShouldRecordMainFrameLoad) {
  using Result = PageRequestSummary::ShouldRecordResourceLoadResult;
  collector_->RecordStartNavigation(GetNextId(), ukm::SourceId(),
                                    GURL("https://irrelevant.com"),
                                    base::TimeTicks::Now());
  auto* page_request_summary =
      collector_->inflight_navigations_.begin()->second.get();

  auto http_request = CreateResourceLoadInfo("http://www.google.com");
  EXPECT_EQ(page_request_summary->ShouldRecordResourceLoad(*http_request),
            Result::kYes);

  auto https_request = CreateResourceLoadInfo("https://www.google.com");
  EXPECT_EQ(page_request_summary->ShouldRecordResourceLoad(*https_request),
            Result::kYes);

  auto file_request = CreateResourceLoadInfo("file://www.google.com");
  EXPECT_EQ(page_request_summary->ShouldRecordResourceLoad(*file_request),
            Result::kNo);

  auto https_request_with_port =
      CreateResourceLoadInfo("https://www.google.com:666");
  EXPECT_EQ(
      page_request_summary->ShouldRecordResourceLoad(*https_request_with_port),
      Result::kNo);
}

TEST_F(LoadingDataCollectorTest, ShouldRecordSubresourceLoadAfterLoadComplete) {
  auto navigation_id = GetNextId();
  GURL url("http://www.google.com");

  collector_->RecordStartNavigation(navigation_id, ukm::SourceId(), url,
                                    base::TimeTicks::Now());
  auto* page_request_summary =
      collector_->inflight_navigations_.begin()->second.get();

  EXPECT_CALL(*mock_predictor_, RecordPageRequestSummaryProxy(_));
  collector_->RecordMainFrameLoadComplete(navigation_id);

  auto http_image_request =
      CreateResourceLoadInfo("http://www.google.com/cat.png",
                             network::mojom::RequestDestination::kImage);
  EXPECT_EQ(page_request_summary->ShouldRecordResourceLoad(*http_image_request),
            PageRequestSummary::ShouldRecordResourceLoadResult::kLowPriority);
}

TEST_F(LoadingDataCollectorTest, ShouldRecordSubresourceLoad) {
  using Result = PageRequestSummary::ShouldRecordResourceLoadResult;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kLoadingOnlyLearnHighPriorityResources);

  collector_->RecordStartNavigation(GetNextId(), ukm::SourceId(),
                                    GURL("https://irrelevant.com"),
                                    base::TimeTicks::Now());
  auto* page_request_summary =
      collector_->inflight_navigations_.begin()->second.get();

  // Protocol.
  auto low_priority_http_image_request = CreateLowPriorityResourceLoadInfo(
      "http://www.google.com/cat.png",
      network::mojom::RequestDestination::kImage);
  EXPECT_EQ(page_request_summary->ShouldRecordResourceLoad(
                *low_priority_http_image_request),
            Result::kLowPriority);

  auto http_image_request =
      CreateResourceLoadInfo("http://www.google.com/cat.png",
                             network::mojom::RequestDestination::kImage);
  EXPECT_EQ(page_request_summary->ShouldRecordResourceLoad(*http_image_request),
            Result::kYes);

  auto https_image_request =
      CreateResourceLoadInfo("https://www.google.com/cat.png",
                             network::mojom::RequestDestination::kImage);
  EXPECT_EQ(
      page_request_summary->ShouldRecordResourceLoad(*https_image_request),
      Result::kYes);

  auto https_image_request_with_port =
      CreateResourceLoadInfo("https://www.google.com:666/cat.png",
                             network::mojom::RequestDestination::kImage);
  EXPECT_EQ(page_request_summary->ShouldRecordResourceLoad(
                *https_image_request_with_port),
            Result::kNo);

  auto file_image_request =
      CreateResourceLoadInfo("file://www.google.com/cat.png",
                             network::mojom::RequestDestination::kImage);
  EXPECT_EQ(page_request_summary->ShouldRecordResourceLoad(*file_image_request),
            Result::kNo);

  // Request destination.
  auto sub_frame_request =
      CreateResourceLoadInfo("http://www.google.com/frame.html",
                             network::mojom::RequestDestination::kIframe);
  EXPECT_EQ(page_request_summary->ShouldRecordResourceLoad(*sub_frame_request),
            Result::kNo);

  auto font_request =
      CreateResourceLoadInfo("http://www.google.com/comic-sans-ms.woff",
                             network::mojom::RequestDestination::kFont);
  EXPECT_EQ(page_request_summary->ShouldRecordResourceLoad(*font_request),
            Result::kYes);

  // From MIME Type.
  auto prefetch_image_request =
      CreateResourceLoadInfo("http://www.google.com/cat.png",
                             network::mojom::RequestDestination::kEmpty);
  prefetch_image_request->mime_type = "image/png";
  EXPECT_EQ(
      page_request_summary->ShouldRecordResourceLoad(*prefetch_image_request),
      Result::kYes);

  auto prefetch_unknown_image_request =
      CreateResourceLoadInfo("http://www.google.com/cat.png",
                             network::mojom::RequestDestination::kEmpty);
  prefetch_unknown_image_request->mime_type = "image/my-wonderful-format";
  EXPECT_EQ(page_request_summary->ShouldRecordResourceLoad(
                *prefetch_unknown_image_request),
            Result::kNo);

  auto prefetch_font_request =
      CreateResourceLoadInfo("http://www.google.com/comic-sans-ms.woff",
                             network::mojom::RequestDestination::kEmpty);
  prefetch_font_request->mime_type = "font/woff";
  EXPECT_EQ(
      page_request_summary->ShouldRecordResourceLoad(*prefetch_font_request),
      Result::kYes);

  auto prefetch_unknown_font_request =
      CreateResourceLoadInfo("http://www.google.com/comic-sans-ms.woff",
                             network::mojom::RequestDestination::kEmpty);
  prefetch_unknown_font_request->mime_type = "font/woff-woff";
  EXPECT_EQ(page_request_summary->ShouldRecordResourceLoad(
                *prefetch_unknown_font_request),
            Result::kNo);
}

// Single navigation that will be recorded. Will check for duplicate
// resources and also for number of resources saved.
TEST_F(LoadingDataCollectorTest, SimpleNavigation) {
  auto navigation_id = GetNextId();
  GURL url("http://www.google.com");

  collector_->RecordStartNavigation(navigation_id, ukm::SourceId(), url,
                                    base::TimeTicks::Now());
  collector_->RecordFinishNavigation(navigation_id, url,
                                     /* is_error_page */ false);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  auto* page_request_summary =
      collector_->inflight_navigations_.begin()->second.get();
  // Ensure that the finish time of the navigation is recorded.
  EXPECT_NE(page_request_summary->navigation_committed, base::TimeTicks::Max());

  // Main frame origin should not be recorded.
  collector_->RecordPreconnectInitiated(navigation_id, url);

  collector_->RecordPreconnectInitiated(navigation_id,
                                        GURL("http://static.google.com"));
  EXPECT_EQ(1u, page_request_summary->preconnect_origins.size());
  EXPECT_NE(page_request_summary->preconnect_origins.end(),
            page_request_summary->preconnect_origins.find(
                url::Origin::Create(GURL("http://static.google.com"))));

  collector_->RecordPrefetchInitiated(navigation_id,
                                      GURL("http://google.com/style1.css"));
  EXPECT_TRUE(page_request_summary->first_prefetch_initiated.has_value());

  base::TimeTicks first_prefetch_initiated =
      collector_->inflight_navigations_.begin()
          ->second->first_prefetch_initiated.value();
  collector_->RecordPrefetchInitiated(navigation_id,
                                      GURL("http://google.com/style2.css"));
  // The first prefetch initiated request time should still hold.
  EXPECT_EQ(first_prefetch_initiated,
            page_request_summary->first_prefetch_initiated.value());

  EXPECT_EQ(2u, page_request_summary->prefetch_urls.size());
  EXPECT_NE(page_request_summary->prefetch_urls.end(),
            page_request_summary->prefetch_urls.find(
                GURL("http://google.com/style1.css")));
  EXPECT_NE(page_request_summary->prefetch_urls.end(),
            page_request_summary->prefetch_urls.find(
                GURL("http://google.com/style2.css")));

  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo("http://www.google.com"));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/style1.css",
                             network::mojom::RequestDestination::kStyle));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/script1.js",
                             network::mojom::RequestDestination::kScript));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/script2.js",
                             network::mojom::RequestDestination::kScript));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/script1.js",
                             network::mojom::RequestDestination::kScript));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/image1.png",
                             network::mojom::RequestDestination::kImage));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/image2.png",
                             network::mojom::RequestDestination::kImage));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(
      CreateResourceLoadInfo("http://google.com/style2.css",
                             network::mojom::RequestDestination::kStyle));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(
      CreateResourceLoadInfo("http://static.google.com/style2-no-store.css",
                             network::mojom::RequestDestination::kStyle,
                             /*always_access_network=*/true));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(CreateResourceLoadInfoWithRedirects(
      {"http://reader.google.com/style.css",
       "http://dev.null.google.com/style.css"},
      network::mojom::RequestDestination::kStyle));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());

  auto summary = CreatePageRequestSummary("http://www.google.com",
                                          "http://www.google.com", resources);
  EXPECT_FALSE(summary.origins.empty());

  EXPECT_CALL(*mock_predictor_, RecordPageRequestSummaryProxy(summary));

  collector_->RecordMainFrameLoadComplete(navigation_id);

  resources.clear();
  resources.push_back(
      CreateResourceLoadInfo("http://static.google.com/style-3.css",
                             network::mojom::RequestDestination::kStyle));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(CreateResourceLoadInfoWithRedirects(
      {"http://reader.google.com/style2.css",
       "http://dev.null.google.com/style2.css"},
      network::mojom::RequestDestination::kStyle));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());

  // Prefetches/preconnects seen after the load event should be ignored and not
  // recorded.
  collector_->RecordPrefetchInitiated(navigation_id,
                                      GURL("http://google.com/style3.css"));
  EXPECT_FALSE(base::Contains(page_request_summary->prefetch_urls,
                              GURL("http://google.com/style3.css")));
  collector_->RecordPreconnectInitiated(
      navigation_id, GURL("https://external.resource.com/style.css"));
  EXPECT_FALSE(base::Contains(
      page_request_summary->preconnect_origins,
      url::Origin::Create(GURL("https://external.resource.com/style.css"))));

  for (const auto& resource_load_info : resources) {
    summary.UpdateOrAddResource(*resource_load_info);
  }

  EXPECT_EQ(*page_request_summary, summary);
}

TEST_F(LoadingDataCollectorTest, SimpleRedirect) {
  auto navigation_id = GetNextId();
  GURL url("http://fb.com/google");

  collector_->RecordStartNavigation(navigation_id, ukm::SourceId(), url,
                                    base::TimeTicks::Now());
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());

  auto main_frame = CreateResourceLoadInfoWithRedirects(
      {"http://fb.com/google", "http://facebook.com/google",
       "https://facebook.com/google"});

  GURL new_url("https://facebook.com/google");
  collector_->RecordFinishNavigation(navigation_id, new_url,
                                     /* is_error_page */ false);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  EXPECT_EQ(url, collector_->inflight_navigations_[navigation_id]->initial_url);
  collector_->RecordResourceLoadComplete(navigation_id, *main_frame);

  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(std::move(main_frame));
  EXPECT_CALL(
      *mock_predictor_,
      RecordPageRequestSummaryProxy(CreatePageRequestSummary(
          "https://facebook.com/google", "http://fb.com/google", resources)));

  collector_->RecordMainFrameLoadComplete(navigation_id);
}

// Tests that RecordNavigationFinish without the corresponding
// RecordNavigationStart works fine.
TEST_F(LoadingDataCollectorTest, RecordStartNavigationMissing) {
  auto navigation_id = GetNextId();
  GURL url("http://bbc.com");
  GURL new_url("https://www.bbc.com");

  collector_->RecordStartNavigation(navigation_id, ukm::SourceId(), url,
                                    base::TimeTicks::Now());

  // collector_->RecordStartNavigtion(navigation_id) is missing.
  collector_->RecordFinishNavigation(navigation_id, new_url,
                                     /* is_error_page */ false);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  EXPECT_EQ(url, collector_->inflight_navigations_[navigation_id]->initial_url);
}

TEST_F(LoadingDataCollectorTest, RecordFailedNavigation) {
  auto navigation_id = GetNextId();
  GURL url("http://bbc.com");

  collector_->RecordStartNavigation(navigation_id, ukm::SourceId(), url,
                                    base::TimeTicks::Now());
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  collector_->RecordFinishNavigation(navigation_id, url,
                                     /* is_error_page */ true);
  EXPECT_TRUE(collector_->inflight_navigations_.empty());
}

TEST_F(LoadingDataCollectorTest, ManyNavigations) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kLoadingPredictorTableConfig,
      {{"max_navigation_lifetime_seconds", "60"}});

  auto navigation_id1 = GetNextId();
  auto navigation_id2 = GetNextId();
  auto navigation_id3 = GetNextId();
  GURL url1("http://www.google.com");
  GURL url2("http://www.google.com");
  GURL url3("http://www.yahoo.com");

  collector_->RecordStartNavigation(navigation_id1, ukm::SourceId(), url1,
                                    base::TimeTicks::Now());
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());

  task_environment_.FastForwardBy(base::Seconds(10));
  collector_->RecordStartNavigation(navigation_id2, ukm::SourceId(), url2,
                                    base::TimeTicks::Now());

  EXPECT_EQ(2U, collector_->inflight_navigations_.size());

  task_environment_.FastForwardBy(base::Seconds(10));
  collector_->RecordStartNavigation(navigation_id3, ukm::SourceId(), url3,
                                    base::TimeTicks::Now());
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());

  task_environment_.FastForwardBy(base::Seconds(51));
  EXPECT_THAT(collector_->inflight_navigations_,
              ElementsAre(Key(navigation_id1), Key(navigation_id2),
                          Key(navigation_id3)));

  collector_->RecordFinishNavigation(navigation_id1, url1,
                                     /*is_error_page=*/false);

  GURL url4("http://www.google.com");
  auto navigation_id4 = GetNextId();
  // Adding this should cause the second navigation to be cleared. The first
  // navigation is kept because it has committed. #3 and #4 have not expired
  // yet.
  collector_->RecordStartNavigation(navigation_id4, ukm::SourceId(), url4,
                                    base::TimeTicks::Now());
  EXPECT_THAT(collector_->inflight_navigations_,
              ElementsAre(Key(navigation_id1), Key(navigation_id3),
                          Key(navigation_id4)));
}

TEST_F(LoadingDataCollectorTest, RecordResourceLoadComplete) {
  // If there is no inflight navigation, nothing happens.
  auto navigation_id = GetNextId();
  GURL url("http://www.google.com");
  auto resource1 =
      CreateResourceLoadInfo("http://google.com/style1.css",
                             network::mojom::RequestDestination::kStyle);
  collector_->RecordResourceLoadComplete(navigation_id, *resource1);
  EXPECT_TRUE(collector_->inflight_navigations_.empty());

  // Add an inflight navigation.
  collector_->RecordStartNavigation(navigation_id, ukm::SourceId(), url,
                                    base::TimeTicks::Now());
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());

  // Now add a few subresources.
  auto resource2 =
      CreateResourceLoadInfo("http://google.com/script1.js",
                             network::mojom::RequestDestination::kScript);
  auto resource3 =
      CreateResourceLoadInfo("http://google.com/script2.js",
                             network::mojom::RequestDestination::kScript);
  collector_->RecordResourceLoadComplete(navigation_id, *resource1);
  collector_->RecordResourceLoadComplete(navigation_id, *resource2);
  collector_->RecordResourceLoadComplete(navigation_id, *resource3);

  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
}

TEST_F(LoadingDataCollectorTest,
       RecordPreconnectInitiatedNoInflightNavigation) {
  // If there is no inflight navigation, nothing happens.
  auto navigation_id = GetNextId();
  collector_->RecordPreconnectInitiated(navigation_id,
                                        GURL("http://google.com/"));
  EXPECT_TRUE(collector_->inflight_navigations_.empty());
}

TEST_F(LoadingDataCollectorTest, RecordPrefetchInitiatedNoInflightNavigation) {
  // If there is no inflight navigation, nothing happens.
  auto navigation_id = GetNextId();
  collector_->RecordPrefetchInitiated(navigation_id,
                                      GURL("http://google.com/style1.css"));
  EXPECT_TRUE(collector_->inflight_navigations_.empty());
}

TEST_F(LoadingDataCollectorTest, RecordPageDestroyed) {
  auto navigation_id = GetNextId();
  const GURL& url = GURL("https://google.com");

  collector_->RecordStartNavigation(navigation_id, ukm::SourceId(), url,
                                    base::TimeTicks::Now());
  EXPECT_FALSE(collector_->inflight_navigations_.empty());

  collector_->RecordFinishNavigation(navigation_id, url,
                                     /*is_error_page=*/false);
  EXPECT_TRUE(collector_->inflight_navigations_[navigation_id]
                  ->navigation_committed.has_value());

  EXPECT_CALL(*mock_predictor_,
              RecordPageRequestSummaryProxy(CreatePageRequestSummary(
                  "https://google.com", "https://google.com", {})));
  collector_->RecordMainFrameLoadComplete(navigation_id);

  // PageRequestSummary for page should still be kept alive after loading
  // finishes.
  EXPECT_FALSE(collector_->inflight_navigations_.empty());

  collector_->RecordPageDestroyed(navigation_id, std::nullopt);
  // PageRequestSummary should be cleared up after the page is destroyed.
  EXPECT_TRUE(collector_->inflight_navigations_.empty());
}

}  // namespace predictors
