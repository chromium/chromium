// Copyright 2014 The Chromium Authors. All rights reserved.
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
      content::ResourceType::kStylesheet, "bogus/mime-type"));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      content::ResourceType::kStylesheet, ""));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      content::ResourceType::kWorker, "text/css"));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      content::ResourceType::kWorker, ""));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      content::ResourceType::kPrefetch, "text/css"));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      content::ResourceType::kPrefetch, "bogus/mime-type"));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      content::ResourceType::kPrefetch, ""));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      content::ResourceType::kPrefetch, "application/font-woff"));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      content::ResourceType::kPrefetch, "font/woff2"));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      content::ResourceType::kXhr, ""));
  EXPECT_FALSE(LoadingDataCollector::IsHandledResourceType(
      content::ResourceType::kXhr, "bogus/mime-type"));
  EXPECT_TRUE(LoadingDataCollector::IsHandledResourceType(
      content::ResourceType::kXhr, "application/javascript"));
}

TEST_F(LoadingDataCollectorTest, ShouldRecordMainFrameLoad) {
  const SessionID kTabId = SessionID::FromSerializedValue(1);
  auto navigation_id = CreateNavigationID(kTabId, "http://www.google.com");
  auto http_request = CreateResourceLoadInfo("http://www.google.com");
  EXPECT_TRUE(
      collector_->ShouldRecordResourceLoad(navigation_id, *http_request));

  auto https_request = CreateResourceLoadInfo("https://www.google.com");
  EXPECT_TRUE(
      collector_->ShouldRecordResourceLoad(navigation_id, *https_request));

  auto file_request = CreateResourceLoadInfo("file://www.google.com");
  EXPECT_FALSE(
      collector_->ShouldRecordResourceLoad(navigation_id, *file_request));

  auto https_request_with_port =
      CreateResourceLoadInfo("https://www.google.com:666");
  EXPECT_FALSE(collector_->ShouldRecordResourceLoad(navigation_id,
                                                    *https_request_with_port));
}

// Resource loaded after FCP event is recorded by default.
TEST_F(LoadingDataCollectorTest, ShouldRecordSubresourceLoadAfterFCP) {
  const SessionID kTabId = SessionID::FromSerializedValue(1);
  auto navigation_id = CreateNavigationID(kTabId, "http://www.google.com");

  collector_->RecordStartNavigation(navigation_id);
  collector_->RecordFirstContentfulPaint(navigation_id, base::TimeTicks::Now());

  // Protocol.
  auto http_image_request = CreateResourceLoadInfo(
      "http://www.google.com/cat.png", content::ResourceType::kImage);
  EXPECT_TRUE(
      collector_->ShouldRecordResourceLoad(navigation_id, *http_image_request));
}

TEST_F(LoadingDataCollectorTest, ShouldRecordSubresourceLoad) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kLoadingOnlyLearnHighPriorityResources);
  const SessionID kTabId = SessionID::FromSerializedValue(1);
  auto navigation_id = CreateNavigationID(kTabId, "http://www.google.com");

  // Protocol.
  auto low_priority_http_image_request = CreateLowPriorityResourceLoadInfo(
      "http://www.google.com/cat.png", content::ResourceType::kImage);
  EXPECT_FALSE(collector_->ShouldRecordResourceLoad(
      navigation_id, *low_priority_http_image_request));

  auto http_image_request = CreateResourceLoadInfo(
      "http://www.google.com/cat.png", content::ResourceType::kImage);
  EXPECT_TRUE(
      collector_->ShouldRecordResourceLoad(navigation_id, *http_image_request));

  auto https_image_request = CreateResourceLoadInfo(
      "https://www.google.com/cat.png", content::ResourceType::kImage);
  EXPECT_TRUE(collector_->ShouldRecordResourceLoad(navigation_id,
                                                   *https_image_request));

  auto https_image_request_with_port = CreateResourceLoadInfo(
      "https://www.google.com:666/cat.png", content::ResourceType::kImage);
  EXPECT_FALSE(collector_->ShouldRecordResourceLoad(
      navigation_id, *https_image_request_with_port));

  auto file_image_request = CreateResourceLoadInfo(
      "file://www.google.com/cat.png", content::ResourceType::kImage);
  EXPECT_FALSE(
      collector_->ShouldRecordResourceLoad(navigation_id, *file_image_request));

  // ResourceType.
  auto sub_frame_request = CreateResourceLoadInfo(
      "http://www.google.com/frame.html", content::ResourceType::kSubFrame);
  EXPECT_FALSE(
      collector_->ShouldRecordResourceLoad(navigation_id, *sub_frame_request));

  auto font_request =
      CreateResourceLoadInfo("http://www.google.com/comic-sans-ms.woff",
                             content::ResourceType::kFontResource);
  EXPECT_TRUE(
      collector_->ShouldRecordResourceLoad(navigation_id, *font_request));

  // From MIME Type.
  auto prefetch_image_request = CreateResourceLoadInfo(
      "http://www.google.com/cat.png", content::ResourceType::kPrefetch);
  prefetch_image_request->mime_type = "image/png";
  EXPECT_TRUE(collector_->ShouldRecordResourceLoad(navigation_id,
                                                   *prefetch_image_request));

  auto prefetch_unknown_image_request = CreateResourceLoadInfo(
      "http://www.google.com/cat.png", content::ResourceType::kPrefetch);
  prefetch_unknown_image_request->mime_type = "image/my-wonderful-format";
  EXPECT_FALSE(collector_->ShouldRecordResourceLoad(
      navigation_id, *prefetch_unknown_image_request));

  auto prefetch_font_request =
      CreateResourceLoadInfo("http://www.google.com/comic-sans-ms.woff",
                             content::ResourceType::kPrefetch);
  prefetch_font_request->mime_type = "font/woff";
  EXPECT_TRUE(collector_->ShouldRecordResourceLoad(navigation_id,
                                                   *prefetch_font_request));

  auto prefetch_unknown_font_request =
      CreateResourceLoadInfo("http://www.google.com/comic-sans-ms.woff",
                             content::ResourceType::kPrefetch);
  prefetch_unknown_font_request->mime_type = "font/woff-woff";
  EXPECT_FALSE(collector_->ShouldRecordResourceLoad(
      navigation_id, *prefetch_unknown_font_request));
}

// Single navigation that will be recorded. Will check for duplicate
// resources and also for number of resources saved.
TEST_F(LoadingDataCollectorTest, SimpleNavigation) {
  const SessionID kTabId = SessionID::FromSerializedValue(1);
  auto navigation_id = CreateNavigationID(kTabId, "http://www.google.com");
  collector_->RecordStartNavigation(navigation_id);
  collector_->RecordFinishNavigation(navigation_id, navigation_id,
                                     /* is_error_page */ false);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());

  std::vector<content::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo("http://www.google.com"));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(CreateResourceLoadInfo(
      "http://google.com/style1.css", content::ResourceType::kStylesheet));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(CreateResourceLoadInfo("http://google.com/script1.js",
                                             content::ResourceType::kScript));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(CreateResourceLoadInfo("http://google.com/script2.js",
                                             content::ResourceType::kScript));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(CreateResourceLoadInfo("http://google.com/script1.js",
                                             content::ResourceType::kScript));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(CreateResourceLoadInfo("http://google.com/image1.png",
                                             content::ResourceType::kImage));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(CreateResourceLoadInfo("http://google.com/image2.png",
                                             content::ResourceType::kImage));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(CreateResourceLoadInfo(
      "http://google.com/style2.css", content::ResourceType::kStylesheet));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(
      CreateResourceLoadInfo("http://static.google.com/style2-no-store.css",
                             content::ResourceType::kStylesheet,
                             /* always_access_network */ true));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());
  resources.push_back(CreateResourceLoadInfoWithRedirects(
      {"http://reader.google.com/style.css",
       "http://dev.null.google.com/style.css"},
      content::ResourceType::kStylesheet));
  collector_->RecordResourceLoadComplete(navigation_id, *resources.back());

  auto summary = CreatePageRequestSummary("http://www.google.com",
                                          "http://www.google.com", resources);
  EXPECT_FALSE(summary.origins.empty());

  EXPECT_CALL(*mock_predictor_,
              RecordPageRequestSummaryProxy(testing::Pointee(summary)));

  collector_->RecordMainFrameLoadComplete(navigation_id);
}

TEST_F(LoadingDataCollectorTest, SimpleRedirect) {
  const SessionID kTabId = SessionID::FromSerializedValue(1);
  auto navigation_id = CreateNavigationID(kTabId, "http://fb.com/google");
  collector_->RecordStartNavigation(navigation_id);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());

  auto main_frame = CreateResourceLoadInfoWithRedirects(
      {"http://fb.com/google", "http://facebook.com/google",
       "https://facebook.com/google"});

  auto new_navigation_id =
      CreateNavigationID(kTabId, "https://facebook.com/google");
  collector_->RecordFinishNavigation(navigation_id, new_navigation_id,
                                     /* is_error_page */ false);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  EXPECT_EQ(navigation_id.main_frame_url,
            collector_->inflight_navigations_[new_navigation_id]->initial_url);
  collector_->RecordResourceLoadComplete(new_navigation_id, *main_frame);

  std::vector<content::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(std::move(main_frame));
  EXPECT_CALL(
      *mock_predictor_,
      RecordPageRequestSummaryProxy(testing::Pointee(CreatePageRequestSummary(
          "https://facebook.com/google", "http://fb.com/google", resources))));

  collector_->RecordMainFrameLoadComplete(new_navigation_id);
}

// Tests that RecordNavigationFinish without the corresponding
// RecordNavigationStart works fine.
TEST_F(LoadingDataCollectorTest, RecordStartNavigationMissing) {
  const SessionID kTabId = SessionID::FromSerializedValue(1);
  auto navigation_id = CreateNavigationID(kTabId, "http://bbc.com");
  auto new_navigation_id = CreateNavigationID(kTabId, "https://www.bbc.com");

  // collector_->RecordStartNavigtion(navigation_id) is missing.
  collector_->RecordFinishNavigation(navigation_id, new_navigation_id,
                                     /* is_error_page */ false);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  EXPECT_EQ(navigation_id.main_frame_url,
            collector_->inflight_navigations_[new_navigation_id]->initial_url);
}

TEST_F(LoadingDataCollectorTest, RecordFailedNavigation) {
  const SessionID kTabId = SessionID::FromSerializedValue(1);
  auto navigation_id = CreateNavigationID(kTabId, "http://bbc.com");

  collector_->RecordStartNavigation(navigation_id);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  collector_->RecordFinishNavigation(navigation_id, navigation_id,
                                     /* is_error_page */ true);
  EXPECT_TRUE(collector_->inflight_navigations_.empty());
}

TEST_F(LoadingDataCollectorTest, ManyNavigations) {
  const SessionID kTabId1 = SessionID::FromSerializedValue(1);
  const SessionID kTabId2 = SessionID::FromSerializedValue(2);
  const SessionID kTabId3 = SessionID::FromSerializedValue(3);
  const SessionID kTabId4 = SessionID::FromSerializedValue(4);

  auto navigation_id1 = CreateNavigationID(kTabId1, "http://www.google.com");
  auto navigation_id2 = CreateNavigationID(kTabId2, "http://www.google.com");
  auto navigation_id3 = CreateNavigationID(kTabId3, "http://www.yahoo.com");

  collector_->RecordStartNavigation(navigation_id1);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
  collector_->RecordStartNavigation(navigation_id2);
  EXPECT_EQ(2U, collector_->inflight_navigations_.size());
  collector_->RecordStartNavigation(navigation_id3);
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());

  // Insert another with same navigation id. It should replace.
  auto navigation_id4 = CreateNavigationID(kTabId1, "http://www.nike.com");
  collector_->RecordStartNavigation(navigation_id4);
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());

  auto navigation_id5 = CreateNavigationID(kTabId2, "http://www.google.com");
  // Change this creation time so that it will go away on the next insert.
  navigation_id5.creation_time =
      base::TimeTicks::Now() - base::TimeDelta::FromDays(1);
  collector_->RecordStartNavigation(navigation_id5);
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());

  auto navigation_id6 = CreateNavigationID(kTabId4, "http://www.shoes.com");
  collector_->RecordStartNavigation(navigation_id6);
  EXPECT_EQ(3U, collector_->inflight_navigations_.size());

  EXPECT_TRUE(collector_->inflight_navigations_.find(navigation_id3) !=
              collector_->inflight_navigations_.end());
  EXPECT_TRUE(collector_->inflight_navigations_.find(navigation_id4) !=
              collector_->inflight_navigations_.end());
  EXPECT_TRUE(collector_->inflight_navigations_.find(navigation_id6) !=
              collector_->inflight_navigations_.end());
}

TEST_F(LoadingDataCollectorTest, RecordResourceLoadComplete) {
  const SessionID kTabId = SessionID::FromSerializedValue(1);
  // If there is no inflight navigation, nothing happens.
  auto navigation_id = CreateNavigationID(kTabId, "http://www.google.com");
  auto resource1 = CreateResourceLoadInfo("http://google.com/style1.css",
                                          content::ResourceType::kStylesheet);
  collector_->RecordResourceLoadComplete(navigation_id, *resource1);
  EXPECT_TRUE(collector_->inflight_navigations_.empty());

  // Add an inflight navigation.
  collector_->RecordStartNavigation(navigation_id);
  EXPECT_EQ(1U, collector_->inflight_navigations_.size());

  // Now add a few subresources.
  auto resource2 = CreateResourceLoadInfo("http://google.com/script1.js",
                                          content::ResourceType::kScript);
  auto resource3 = CreateResourceLoadInfo("http://google.com/script2.js",
                                          content::ResourceType::kScript);
  collector_->RecordResourceLoadComplete(navigation_id, *resource1);
  collector_->RecordResourceLoadComplete(navigation_id, *resource2);
  collector_->RecordResourceLoadComplete(navigation_id, *resource3);

  EXPECT_EQ(1U, collector_->inflight_navigations_.size());
}

}  // namespace predictors
