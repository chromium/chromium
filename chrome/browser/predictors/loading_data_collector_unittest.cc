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
  LoadingDataCollectorTest() : profile_(std::make_unique<TestingProfile>()) {
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
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      network::mojom::RequestDestination::kStyle, "bogus/mime-type"));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      network::mojom::RequestDestination::kStyle, ""));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      network::mojom::RequestDestination::kWorker, "text/css"));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      network::mojom::RequestDestination::kWorker, ""));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      network::mojom::RequestDestination::kEmpty, "text/css"));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      network::mojom::RequestDestination::kEmpty, "bogus/mime-type"));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      network::mojom::RequestDestination::kEmpty, ""));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      network::mojom::RequestDestination::kEmpty, "application/font-woff"));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      network::mojom::RequestDestination::kEmpty, "font/woff2"));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      network::mojom::RequestDestination::kEmpty, "application/javascript"));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      network::mojom::RequestDestination::kDocument, "text/html"));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      network::mojom::RequestDestination::kDocument, ""));
}

TEST_F(LoadingDataCollectorTest, ShouldRecordMainFrameLoad) {
  auto http_request = CreateResourceLoadInfo("http://www.google.com");
  EXPECT_TRUE(collector_->ShouldRecordResourceLoad(*http_request));

  auto https_request = CreateResourceLoadInfo("https://www.google.com");
  EXPECT_TRUE(collector_->ShouldRecordResourceLoad(*https_request));

  auto file_request = CreateResourceLoadInfo("file://www.google.com");
  EXPECT_FALSE(collector_->ShouldRecordResourceLoad(*file_request));

  auto https_request_with_port =
      CreateResourceLoadInfo("https://www.google.com:666");
  EXPECT_FALSE(collector_->ShouldRecordResourceLoad(*https_request_with_port));
}

// Resource loaded after FCP event is recorded by default.
TEST_F(LoadingDataCollectorTest, ShouldRecordSubresourceLoadAfterFCP) {
  auto navigation_id = GetNextId();
  GURL url("http://www.google.com");

  collector_->RecordStartNavigation(navigation_id, ukm::SourceId(), url,
                                    base::TimeTicks::Now());
  collector_->RecordFirstContentfulPaint(navigation_id, base::TimeTicks::Now());

  // Protocol.
  auto http_image_request =
      CreateResourceLoadInfo("http://www.google.com/cat.png",
                             network::mojom::RequestDestination::kImage);
  EXPECT_TRUE(collector_->ShouldRecordResourceLoad(*http_image_request));
}

TEST_F(LoadingDataCollectorTest, ShouldRecordSubresourceLoad) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kLoadingOnlyLearnHighPriorityResources);

  // Protocol.
  auto low_priority_http_image_request = CreateLowPriorityResourceLoadInfo(
      "http://www.google.com/cat.png",
      network::mojom::RequestDestination::kImage);
  EXPECT_FALSE(
      collector_->ShouldRecordResourceLoad(*low_priority_http_image_request));

  auto http_image_request =
      CreateResourceLoadInfo("http://www.google.com/cat.png",
                             network::mojom::RequestDestination::kImage);
  EXPECT_TRUE(collector_->ShouldRecordResourceLoad(*http_image_request));

  auto https_image_request =
      CreateResourceLoadInfo("https://www.google.com/cat.png",
                             network::mojom::RequestDestination::kImage);
  EXPECT_TRUE(collector_->ShouldRecordResourceLoad(*https_image_request));

  auto https_image_request_with_port =
      CreateResourceLoadInfo("https://www.google.com:666/cat.png",
                             network::mojom::RequestDestination::kImage);
  EXPECT_FALSE(
      collector_->ShouldRecordResourceLoad(*https_image_request_with_port));

  auto file_image_request =
      CreateResourceLoadInfo("file://www.google.com/cat.png",
                             network::mojom::RequestDestination::kImage);
  EXPECT_FALSE(collector_->ShouldRecordResourceLoad(*file_image_request));

  // Request destination.
  auto sub_frame_request =
      CreateResourceLoadInfo("http://www.google.com/frame.html",
                             network::mojom::RequestDestination::kIframe);
  EXPECT_FALSE(collector_->ShouldRecordResourceLoad(*sub_frame_request));

  auto font_request =
      CreateResourceLoadInfo("http://www.google.com/comic-sans-ms.woff",
                             network::mojom::RequestDestination::kFont);
  EXPECT_TRUE(collector_->ShouldRecordResourceLoad(*font_request));

  // From MIME Type.
  auto prefetch_image_request =
      CreateResourceLoadInfo("http://www.google.com/cat.png",
                             network::mojom::RequestDestination::kEmpty);
  prefetch_image_request->mime_type = "image/png";
  EXPECT_TRUE(collector_->ShouldRecordResourceLoad(*prefetch_image_request));

  auto prefetch_unknown_image_request =
      CreateResourceLoadInfo("http://www.google.com/cat.png",
                             network::mojom::RequestDestination::kEmpty);
  prefetch_unknown_image_request->mime_type = "image/my-wonderful-format";
  EXPECT_FALSE(
      collector_->ShouldRecordResourceLoad(*prefetch_unknown_image_request));

  auto prefetch_font_request =
      CreateResourceLoadInfo("http://www.google.com/comic-sans-ms.woff",
                             network::mojom::RequestDestination::kEmpty);
  prefetch_font_request->mime_type = "font/woff";
  EXPECT_TRUE(collector_->ShouldRecordResourceLoad(*prefetch_font_request));

  auto prefetch_unknown_font_request =
      CreateResourceLoadInfo("http://www.google.com/comic-sans-ms.woff",
                             network::mojom::RequestDestination::kEmpty);
  prefetch_unknown_font_request->mime_type = "font/woff-woff";
  EXPECT_FALSE(
      collector_->ShouldRecordResourceLoad(*prefetch_unknown_font_request));
}

// Single navigation that will be recorded. Will check for duplicate
// resources and also for number of resources saved.
TEST_F(LoadingDataCollectorTest, SimpleNavigation) {
  auto navigation_id = GetNextId();
  GURL url("http://www.google.com");

  collector_->RecordStartNavigation(navigation_id, ukm::SourceId(), url,
                                    base::TimeTicks::Now());
  collector_->RecordFinishNavigation(navigation_id, url, url,
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

  EXPECT_CALL(*mock_predictor_,
              RecordPageRequestSummaryProxy(testing::Pointee(summary)));

  collector_->RecordMainFrameLoadComplete(navigation_id, absl::nullopt);
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
  collector_->RecordFinishNavigation(navigation_id, url, new_url,
                                     /* is_error_page */ false);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  EXPECT_EQ(url, collector_->inflight_navigations_[navigation_id]->initial_url);
  collector_->RecordResourceLoadComplete(navigation_id, *main_frame);

  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(std::move(main_frame));
  EXPECT_CALL(
      *mock_predictor_,
      RecordPageRequestSummaryProxy(testing::Pointee(CreatePageRequestSummary(
          "https://facebook.com/google", "http://fb.com/google", resources))));

  collector_->RecordMainFrameLoadComplete(navigation_id, absl::nullopt);
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
  collector_->RecordFinishNavigation(navigation_id, url, new_url,
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
  collector_->RecordFinishNavigation(navigation_id, url, url,
                                     /* is_error_page */ true);
  EXPECT_TRUE(collector_->inflight_navigations_.empty());
}

TEST_F(LoadingDataCollectorTest, ManyNavigations) {
  auto navigation_id1 = GetNextId();
  auto navigation_id2 = GetNextId();
  auto navigation_id3 = GetNextId();
  GURL url1("http://www.google.com");
  GURL url2("http://www.google.com");
  GURL url3("http://www.yahoo.com");

  collector_->RecordStartNavigation(navigation_id1, ukm::SourceId(), url1,
                                    base::TimeTicks::Now());
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  collector_->RecordStartNavigation(navigation_id2, ukm::SourceId(), url2,
                                    base::TimeTicks::Now());
  EXPECT_EQ(2U, collector_->inflight_navigations_.size());
  collector_->RecordStartNavigation(navigation_id3, ukm::SourceId(), url3,
                                    base::TimeTicks::Now());
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());

  // Insert another with same navigation id. It should replace.
  GURL url4("http://www.nike.com");
  collector_->RecordStartNavigation(navigation_id1, ukm::SourceId(), url4,
                                    base::TimeTicks::Now());
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());

  GURL url5("http://www.google.com");
  // Change this creation time so that it will go away on the next insert.
  collector_->RecordStartNavigation(navigation_id2, ukm::SourceId(), url5,
                                    base::TimeTicks::Now() - base::Days(1));
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());

  auto navigation_id6 = GetNextId();
  GURL url6("http://www.shoes.com");
  collector_->RecordStartNavigation(navigation_id6, ukm::SourceId(), url6,
                                    base::TimeTicks::Now());
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());

  EXPECT_TRUE(collector_->inflight_navigations_.find(navigation_id1) !=
              collector_->inflight_navigations_.end());
  EXPECT_TRUE(collector_->inflight_navigations_.find(navigation_id3) !=
              collector_->inflight_navigations_.end());
  EXPECT_TRUE(collector_->inflight_navigations_.find(navigation_id6) !=
              collector_->inflight_navigations_.end());
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

}  // namespace predictors
