// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/image_fetcher.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/lobster/lobster_test_utils.h"
#include "chrome/browser/ash/lobster/mock/mock_snapper_provider.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr int kPreviewImageDimensionSize = 512;
constexpr int kFullImageDimensionSize = 1024;
constexpr int kFakeBaseGenerationSeed = 10;

class ImageFetcherTest : public testing::Test {
 public:
  ImageFetcherTest() {}
  ImageFetcherTest(const ImageFetcherTest&) = delete;
  ImageFetcherTest& operator=(const ImageFetcherTest&) = delete;

  ~ImageFetcherTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(ImageFetcherTest, RequestCandidatesCallSnapperProvider) {
  MockSnapperProvider snapper_provider;
  LobsterCandidateIdGenerator id_generator;
  ImageFetcher image_fetcher(&snapper_provider, &id_generator);

  EXPECT_CALL(
      snapper_provider,
      Call(
          base::test::EqualsProto(CreateTestMantaRequest(
              /*query=*/"a lovely cake", /*seed=*/std::nullopt, /*size=*/
              gfx::Size(kPreviewImageDimensionSize, kPreviewImageDimensionSize),
              /*num_outputs=*/2)),
          testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const manta::proto::Request& request,
             net::NetworkTrafficAnnotationTag traffic_annotation,
             manta::MantaProtoResponseCallback done_callback) {
            std::move(done_callback)
                .Run(CreateFakeMantaResponse(
                         2, gfx::Size(kPreviewImageDimensionSize,
                                      kPreviewImageDimensionSize)),
                     {.status_code = manta::MantaStatusCode::kOk,
                      .message = ""});
          }));

  base::test::TestFuture<const ash::LobsterResult&> future;

  image_fetcher.RequestCandidates(
      /*query=*/"a lovely cake",
      /*num_candidates=*/2, future.GetCallback());

  EXPECT_THAT(
      future.Get().value(),
      testing::ElementsAre(
          EqLobsterImageCandidate(/*expected_id=*/0,
                                  /*expected_bitmap=*/
                                  CreateTestBitmap(kPreviewImageDimensionSize,
                                                   kPreviewImageDimensionSize),
                                  /*expected_generation_seed=*/10,
                                  /*expected_query=*/"a lovely cake"),
          EqLobsterImageCandidate(/*expected_id=*/1,
                                  /*expected_bitmap=*/
                                  CreateTestBitmap(kPreviewImageDimensionSize,
                                                   kPreviewImageDimensionSize),
                                  /*expected_generation_seed=*/11,
                                  /*expected_query=*/"a lovely cake")));
}

TEST_F(ImageFetcherTest, RequestFullSizeCandidatesCallSnapperProvider) {
  MockSnapperProvider snapper_provider;
  LobsterCandidateIdGenerator id_generator;
  ImageFetcher image_fetcher(&snapper_provider, &id_generator);

  EXPECT_CALL(
      snapper_provider,
      Call(base::test::EqualsProto(CreateTestMantaRequest(
               /*query=*/"a lovely cake",
               /*seed=*/kFakeBaseGenerationSeed, /*size=*/
               gfx::Size(kFullImageDimensionSize, kFullImageDimensionSize),
               /*num_outputs=*/1)),
           testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const manta::proto::Request& request,
             net::NetworkTrafficAnnotationTag traffic_annotation,
             manta::MantaProtoResponseCallback done_callback) {
            std::move(done_callback)
                .Run(CreateFakeMantaResponse(
                         1, gfx::Size(kFullImageDimensionSize,
                                      kFullImageDimensionSize)),
                     {.status_code = manta::MantaStatusCode::kOk,
                      .message = ""});
          }));

  base::test::TestFuture<const ash::LobsterResult&> future;

  image_fetcher.RequestFullSizeCandidate(
      /*query=*/"a lovely cake", /*seed=*/kFakeBaseGenerationSeed,
      future.GetCallback());

  EXPECT_THAT(
      future.Get().value(),
      testing::ElementsAre(EqLobsterImageCandidate(
          /*expected_id=*/0,
          /*expected_bitmap=*/
          CreateTestBitmap(kFullImageDimensionSize, kFullImageDimensionSize),
          /*expected_generation_seed=*/kFakeBaseGenerationSeed,
          /*expected_query=*/"a lovely cake")));
}

TEST_F(ImageFetcherTest,
       RequestCandidatesReturnsUnknownErrorIfMantaProviderHasAGenericError) {
  MockSnapperProvider snapper_provider;
  LobsterCandidateIdGenerator id_generator;
  ImageFetcher image_fetcher(&snapper_provider, &id_generator);

  EXPECT_CALL(
      snapper_provider,
      Call(
          base::test::EqualsProto(CreateTestMantaRequest(
              /*query=*/"a sweet candy", /*seed=*/std::nullopt, /*size=*/
              gfx::Size(kPreviewImageDimensionSize, kPreviewImageDimensionSize),
              /*num_outputs=*/2)),
          testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const manta::proto::Request& request,
             net::NetworkTrafficAnnotationTag traffic_annotation,
             manta::MantaProtoResponseCallback done_callback) {
            std::move(done_callback)
                .Run(std::make_unique<manta::proto::Response>(),
                     {.status_code = manta::MantaStatusCode::kGenericError,
                      .message = "generic error"});
          }));

  base::test::TestFuture<const ash::LobsterResult&> future;

  image_fetcher.RequestCandidates(
      /*query=*/"a sweet candy",
      /*num_candidates=*/2, future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ(
      future.Get().error(),
      ash::LobsterError(ash::LobsterErrorCode::kUnknown, "generic error"));
}

}  // namespace
