// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_candidate_resizer.h"

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

constexpr int kFullImageDimensionLength = 1024;
constexpr int kFakeBaseGenerationSeed = 10;

class LobsterCandidateResizerTest : public testing::Test {
 public:
  LobsterCandidateResizerTest() {}
  LobsterCandidateResizerTest(const LobsterCandidateResizerTest&) = delete;
  LobsterCandidateResizerTest& operator=(const LobsterCandidateResizerTest&) =
      delete;

  ~LobsterCandidateResizerTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(LobsterCandidateResizerTest, InflateImageCallsSnapperProvider) {
  MockSnapperProvider snapper_provider;
  LobsterCandidateIdGenerator id_generator;
  ImageFetcher image_fetcher(&snapper_provider, &id_generator);
  LobsterCandidateResizer resizer(&image_fetcher);

  EXPECT_CALL(
      snapper_provider,
      Call(base::test::EqualsProto(CreateTestMantaRequest(
               /*query=*/"a nice strawberry",
               /*seed=*/kFakeBaseGenerationSeed, /*size=*/
               gfx::Size(kFullImageDimensionLength, kFullImageDimensionLength),
               /*num_outputs=*/1)),
           testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const manta::proto::Request& request,
             net::NetworkTrafficAnnotationTag traffic_annotation,
             manta::MantaProtoResponseCallback done_callback) {
            std::move(done_callback)
                .Run(CreateFakeMantaResponse(
                         1, gfx::Size(kFullImageDimensionLength,
                                      kFullImageDimensionLength)),
                     {.status_code = manta::MantaStatusCode::kOk,
                      .message = ""});
          }));

  base::test::TestFuture<const ash::LobsterResult&> future;

  resizer.InflateImage(
      /*seed=*/kFakeBaseGenerationSeed, /*query=*/"a nice strawberry",
      future.GetCallback());

  EXPECT_THAT(future.Get().value(),
              testing::ElementsAre(EqLobsterImageCandidate(
                  /*expected_id=*/0,
                  /*expected_bitmap=*/
                  CreateTestBitmap(kFullImageDimensionLength,
                                   kFullImageDimensionLength),
                  /*expected_generation_seed=*/kFakeBaseGenerationSeed,
                  /*expected_query=*/"a nice strawberry")));
}

TEST_F(LobsterCandidateResizerTest,
       InflateImageReturnsErrorIfSnapperProviderReceivesErrorResponse) {
  MockSnapperProvider snapper_provider;
  LobsterCandidateIdGenerator id_generator;
  ImageFetcher image_fetcher(&snapper_provider, &id_generator);
  LobsterCandidateResizer resizer(&image_fetcher);

  EXPECT_CALL(
      snapper_provider,
      Call(base::test::EqualsProto(CreateTestMantaRequest(
               /*query=*/"a nice strawberry",
               /*seed=*/kFakeBaseGenerationSeed, /*size=*/
               gfx::Size(kFullImageDimensionLength, kFullImageDimensionLength),
               /*num_outputs=*/1)),
           testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const manta::proto::Request& request,
             net::NetworkTrafficAnnotationTag traffic_annotation,
             manta::MantaProtoResponseCallback done_callback) {
            std::move(done_callback)
                .Run(CreateFakeMantaResponse(
                         0, gfx::Size(kFullImageDimensionLength,
                                      kFullImageDimensionLength)),
                     {.status_code = manta::MantaStatusCode::kGenericError,
                      .message = "dummy error"});
          }));

  base::test::TestFuture<const ash::LobsterResult&> future;

  resizer.InflateImage(
      /*seed=*/kFakeBaseGenerationSeed, /*query=*/"a nice strawberry",
      future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(),
            ash::LobsterError(/*status_code=*/ash::LobsterErrorCode::kUnknown,
                              "dummy error"));
}

TEST_F(LobsterCandidateResizerTest,
       InflateImageReturnsErrorIfSnapperProviderReturnsEmptyResponse) {
  MockSnapperProvider snapper_provider;
  LobsterCandidateIdGenerator id_generator;
  ImageFetcher image_fetcher(&snapper_provider, &id_generator);
  LobsterCandidateResizer resizer(&image_fetcher);

  EXPECT_CALL(
      snapper_provider,
      Call(base::test::EqualsProto(CreateTestMantaRequest(
               /*query=*/"a nice strawberry",
               /*seed=*/kFakeBaseGenerationSeed, /*size=*/
               gfx::Size(kFullImageDimensionLength, kFullImageDimensionLength),
               /*num_outputs=*/1)),
           testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const manta::proto::Request& request,
             net::NetworkTrafficAnnotationTag traffic_annotation,
             manta::MantaProtoResponseCallback done_callback) {
            std::move(done_callback)
                .Run(CreateFakeMantaResponse(
                         0, gfx::Size(kFullImageDimensionLength,
                                      kFullImageDimensionLength)),
                     {.status_code = manta::MantaStatusCode::kOk,
                      .message = ""});
          }));

  base::test::TestFuture<const ash::LobsterResult&> future;

  resizer.InflateImage(
      /*seed=*/kFakeBaseGenerationSeed, /*query=*/"a nice strawberry",
      future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(),
            ash::LobsterError(/*status_code=*/ash::LobsterErrorCode::kUnknown,
                              /*message=*/"empty candidate response"));
}

}  // namespace
