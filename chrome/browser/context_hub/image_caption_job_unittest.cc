// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/image_caption_job.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ai/features.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "chrome/browser/context_hub/test/test_image_caption_model_broker.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace context_hub {

namespace {

using ::testing::_;

constexpr char kTestImageUrl[] = "https://example.com/photo.png";

class ImageCaptionJobTest : public testing::Test {
 protected:
  static MemoryBankEntry MakeImageEntry(
      const GURL& image_url = GURL(kTestImageUrl)) {
    return MemoryBankEntry(MemoryBankType::kImage, GURL("https://example.com"),
                           "Page Title", /*selected_text=*/std::nullopt,
                           image_url);
  }

  std::unique_ptr<ImageCaptionJob> CreateJob(
      const GURL& image_url,
      ImageCaptionJob::CaptionCallback callback) {
    return std::make_unique<ImageCaptionJob>(
        MakeImageEntry(image_url), &mock_image_fetcher_,
        model_broker_client_.get(), TRAFFIC_ANNOTATION_FOR_TESTS,
        std::move(callback));
  }

  std::tuple<MemoryBankEntry, std::string> RunCaptionJob(
      const GURL& image_url = GURL(kTestImageUrl)) {
    base::test::TestFuture<MemoryBankEntry, std::string> future;
    std::unique_ptr<ImageCaptionJob> job =
        CreateJob(image_url, future.GetCallback());
    EXPECT_FALSE(job->is_running());
    job->Start();
    EXPECT_TRUE(job->is_running());
    std::tuple<MemoryBankEntry, std::string> result = future.Take();
    EXPECT_FALSE(job->is_running());
    model_broker_.FlushForTesting();
    return result;
  }

  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<image_fetcher::MockImageFetcher> mock_image_fetcher_;
  TestImageCaptionModelBroker model_broker_{
      "A steaming bowl of tonkotsu ramen with sliced pork and green onions."};
  std::unique_ptr<optimization_guide::ModelBrokerClient> model_broker_client_ =
      model_broker_.CreateClient();
};

TEST_F(ImageCaptionJobTest, GeneratesCaptionAndReportsHealthyCompletion) {
  model_broker_.SetDefaultUseCase("prompt_api_v3");
  EXPECT_CALL(mock_image_fetcher_,
              FetchImageAndData_(GURL(kTestImageUrl), _, _, _))
      .WillOnce(FulfillImageFetch(gfx::test::CreateImage(16, 16)));

  auto [entry, caption] = RunCaptionJob();

  EXPECT_EQ(entry.image_url, GURL(kTestImageUrl));
  EXPECT_EQ(
      caption,
      "A steaming bowl of tonkotsu ramen with sliced pork and green onions.");
  EXPECT_EQ(model_broker_.last_subscribed_use_case(), "prompt_api_v3");
  EXPECT_TRUE(model_broker_.received_image_input());
  EXPECT_EQ(model_broker_.received_bitmap_size(), gfx::Size(16, 16));
  EXPECT_TRUE(model_broker_.reported_healthy_completion());
  EXPECT_EQ(
      model_broker_.received_prompt_text(),
      "In 30 words or less, describe the main subject and setting of this "
      "image, and transcribe any prominent visible text.");
}

TEST_F(ImageCaptionJobTest, OversizedImage_DownscalesPreservingAspectRatio) {
  EXPECT_CALL(mock_image_fetcher_,
              FetchImageAndData_(GURL(kTestImageUrl), _, _, _))
      .WillOnce(FulfillImageFetch(gfx::test::CreateImage(2048, 1536)));

  auto [entry, caption] = RunCaptionJob();

  EXPECT_FALSE(caption.empty());
  EXPECT_TRUE(model_broker_.received_image_input());
  EXPECT_EQ(model_broker_.received_bitmap_size(), gfx::Size(1024, 768));
}

TEST_F(ImageCaptionJobTest,
       ExtremeAspectRatioImage_ClampsMinorDimensionToOnePixel) {
  EXPECT_CALL(mock_image_fetcher_,
              FetchImageAndData_(GURL(kTestImageUrl), _, _, _))
      .WillOnce(FulfillImageFetch(gfx::test::CreateImage(4096, 1)));

  auto [entry, caption] = RunCaptionJob();

  EXPECT_FALSE(caption.empty());
  EXPECT_TRUE(model_broker_.received_image_input());
  EXPECT_EQ(model_broker_.received_bitmap_size(), gfx::Size(1024, 1));
}

TEST_F(ImageCaptionJobTest,
       FoundationalModelExperimentalUseCase_SubscribesToExperimentalUseCase) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAIApiFoundationalModel, {{"model_version", "v4"}});
  model_broker_.SetDefaultUseCase("prompt_api");
  model_broker_.SetExperimentalUseCase("v4", "prompt_api_gemma4");
  model_broker_.set_available_use_case("prompt_api_gemma4");

  EXPECT_CALL(mock_image_fetcher_,
              FetchImageAndData_(GURL(kTestImageUrl), _, _, _))
      .WillOnce(FulfillImageFetch(gfx::test::CreateImage(16, 16)));

  auto [entry, caption] = RunCaptionJob();

  EXPECT_FALSE(caption.empty());
  EXPECT_EQ(model_broker_.last_subscribed_use_case(), "prompt_api_gemma4");
  EXPECT_TRUE(model_broker_.received_image_input());
}

TEST_F(ImageCaptionJobTest,
       UnconfiguredFeatureConfig_FallsBackToDefaultPromptApiUseCase) {
  EXPECT_CALL(mock_image_fetcher_,
              FetchImageAndData_(GURL(kTestImageUrl), _, _, _))
      .WillOnce(FulfillImageFetch(gfx::test::CreateImage(16, 16)));

  auto [entry, caption] = RunCaptionJob();

  EXPECT_FALSE(caption.empty());
  EXPECT_EQ(model_broker_.last_subscribed_use_case(), "prompt_api");
}

TEST_F(ImageCaptionJobTest,
       ModelUnavailable_ReturnsEmptyCaptionWithoutFetchingImage) {
  model_broker_.SetAvailable(false);
  EXPECT_CALL(mock_image_fetcher_, FetchImageAndData_).Times(0);

  auto [entry, caption] = RunCaptionJob();

  EXPECT_TRUE(caption.empty());
}

TEST_F(ImageCaptionJobTest, ImageFetchFails_ReturnsEmptyCaption) {
  EXPECT_CALL(mock_image_fetcher_,
              FetchImageAndData_(GURL(kTestImageUrl), _, _, _))
      .WillOnce(FulfillImageFetch(gfx::Image()));

  auto [entry, caption] = RunCaptionJob();

  EXPECT_TRUE(caption.empty());
  EXPECT_FALSE(model_broker_.received_image_input());
}

TEST_F(ImageCaptionJobTest,
       ModelBecomesPendingAssetsDuringFetch_ReturnsEmptyCaptionImmediately) {
  HeldImageFetch held_fetch;
  EXPECT_CALL(mock_image_fetcher_,
              FetchImageAndData_(GURL(kTestImageUrl), _, _, _))
      .WillOnce(held_fetch.Hold());

  base::test::TestFuture<MemoryBankEntry, std::string> future;
  std::unique_ptr<ImageCaptionJob> job =
      CreateJob(GURL(kTestImageUrl), future.GetCallback());
  job->Start();
  ASSERT_TRUE(held_fetch.WaitUntilStarted());

  model_broker_.SetUnavailable(
      optimization_guide::mojom::ModelUnavailableReason::kPendingAssets);
  held_fetch.Complete(gfx::test::CreateImage(16, 16));

  auto [entry, caption] = future.Take();
  EXPECT_FALSE(job->is_running());
  EXPECT_TRUE(caption.empty());
  EXPECT_FALSE(model_broker_.received_image_input());
}

TEST_F(ImageCaptionJobTest,
       ModelDisconnectsDuringGeneration_ReturnsEmptyCaption) {
  model_broker_.set_disconnect_on_generate(true);
  EXPECT_CALL(mock_image_fetcher_,
              FetchImageAndData_(GURL(kTestImageUrl), _, _, _))
      .WillOnce(FulfillImageFetch(gfx::test::CreateImage(16, 16)));

  auto [entry, caption] = RunCaptionJob();

  EXPECT_TRUE(caption.empty());
  EXPECT_FALSE(model_broker_.reported_healthy_completion());
}

}  // namespace

}  // namespace context_hub
