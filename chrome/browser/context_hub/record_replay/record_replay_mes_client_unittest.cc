// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/record_replay_mes_client.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/context_hub/record_replay/recording_artifacts.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

namespace context_hub {
namespace {

using ::base::test::TestFuture;
using ::browser::context_hub::mojom::SkillRecordingActionType;
using ::testing::_;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::WithArg;

std::string CreateTestJpegBytes() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(2, 2);
  bitmap.eraseColor(SK_ColorBLUE);
  std::optional<std::vector<uint8_t>> encoded =
      gfx::JPEGCodec::Encode(bitmap, /*quality=*/80);
  CHECK(encoded.has_value());
  return std::string(base::as_string_view(encoded.value()));
}

optimization_guide::OptimizationGuideModelExecutionResult
CreateSkillGenerationMesResult(std::string_view skill_instructions) {
  optimization_guide::proto::ContextHubResponse response;
  response.mutable_memory_bank_chat_response()->set_text_response(
      skill_instructions);
  return optimization_guide::OptimizationGuideModelExecutionResult(
      base::ok(optimization_guide::AnyWrapProto(response)), nullptr);
}

class RecordReplayMesClientTest : public testing::Test {
 public:
  RecordReplayMesClientTest() = default;
  RecordReplayMesClientTest(const RecordReplayMesClientTest&) = delete;
  RecordReplayMesClientTest& operator=(const RecordReplayMesClientTest&) =
      delete;
  ~RecordReplayMesClientTest() override = default;

 protected:
  void ExpectExecuteModelAndRespond(
      std::string_view response_text,
      base::OnceCallback<
          void(const optimization_guide::proto::ContextHubRequest&,
               const optimization_guide::ModelExecutionOptions&)> verify) {
    std::string response_str(response_text);
    EXPECT_CALL(
        mock_model_executor_,
        ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub,
                     _, _, _))
        .WillOnce([response_str = std::move(response_str),
                   verify = std::move(verify)](
                      optimization_guide::ModelBasedCapabilityKey feature,
                      const google::protobuf::MessageLite& request_metadata,
                      const optimization_guide::ModelExecutionOptions& options,
                      optimization_guide::
                          OptimizationGuideModelExecutionResultCallback
                              callback) mutable {
          const auto& request =
              static_cast<const optimization_guide::proto::ContextHubRequest&>(
                  request_metadata);
          std::move(verify).Run(request, options);
          std::move(callback).Run(CreateSkillGenerationMesResult(response_str),
                                  nullptr);
        });
  }

  base::test::TaskEnvironment task_environment_;
  NiceMock<optimization_guide::MockRemoteModelExecutor> mock_model_executor_;
  RecordReplayMesClient client_{&mock_model_executor_};
};

TEST_F(RecordReplayMesClientTest, GenerateSkillFromRecording_Success) {
  ExpectExecuteModelAndRespond(
      "  Skill Title: Submit Form\n1. Click the Submit button.\n\n",
      base::BindOnce(
          [](const optimization_guide::proto::ContextHubRequest& request,
             const optimization_guide::ModelExecutionOptions& options) {
            EXPECT_EQ(options.execution_timeout, base::Seconds(60));
            EXPECT_EQ(request.request_type(),
                      optimization_guide::proto::
                          CONTEXT_HUB_REQUEST_TYPE_MEMORY_BANK_CHAT);
            EXPECT_THAT(request.user_command(),
                        AllOf(HasSubstr("--- Transcript 1 ---\nText: Click the "
                                        "submit button"),
                              HasSubstr("Clicked Submit"),
                              HasSubstr("https://example.com/form"),
                              HasSubstr("action_1_screenshot.png"),
                              HasSubstr("--- Transcript 2 ---\nText: Now "
                                        "confirm it"),
                              HasSubstr("Clicked Confirm"),
                              Not(HasSubstr("action_2_screenshot")),
                              HasSubstr("Clicked Retry"),
                              Not(HasSubstr("action_3_screenshot"))));
            EXPECT_LT(request.user_command().find("Click the submit button"),
                      request.user_command().find("Clicked Submit"));
            EXPECT_LT(request.user_command().find("--- Transcript 1 ---"),
                      request.user_command().find("--- Transcript 2 ---"));

            ASSERT_EQ(request.entry_items_size(), 1);
            const optimization_guide::proto::MemoryBankEntry& mb_entry =
                request.entry_items(0).memory_bank_entry();
            EXPECT_EQ(mb_entry.type(),
                      optimization_guide::proto::MEMORY_BANK_TYPE_IMAGE);
            EXPECT_EQ(mb_entry.tab_title(), "action_1_screenshot.png");
            EXPECT_EQ(mb_entry.url(), "https://example.com/form");

            SkBitmap decoded_png = gfx::PNGCodec::Decode(
                base::as_byte_span(mb_entry.image_bytes()));
            EXPECT_FALSE(decoded_png.isNull());
            EXPECT_TRUE(decoded_png.isOpaque());
            EXPECT_EQ(decoded_png.width(), 2);
            EXPECT_EQ(decoded_png.height(), 2);
          }));

  RecordingArtifacts artifacts;
  artifacts.actions.emplace_back(SkillRecordingActionType::kLeftClick,
                                 "Clicked Submit", base::Milliseconds(150));
  artifacts.actions.back().current_url = GURL("https://example.com/form");
  artifacts.actions.back().screenshot_jpeg_bytes = CreateTestJpegBytes();
  // Action without a screenshot or URL.
  artifacts.actions.emplace_back(SkillRecordingActionType::kLeftClick,
                                 "Clicked Confirm", base::Milliseconds(250));
  // Action with corrupt JPEG bytes (should keep text metadata, drop image).
  artifacts.actions.emplace_back(SkillRecordingActionType::kLeftClick,
                                 "Clicked Retry", base::Milliseconds(350));
  artifacts.actions.back().screenshot_jpeg_bytes = "invalid_jpeg_bytes";
  // Append transcripts out of timestamp order to verify monotonic 1-based
  // numbering after sorting.
  artifacts.transcripts.emplace_back("Now confirm it", base::Milliseconds(200));
  artifacts.transcripts.emplace_back("Click the submit button",
                                     base::Milliseconds(100));

  TestFuture<std::optional<std::string>> future;
  client_.GenerateSkillFromRecording(artifacts, future.GetCallback());
  EXPECT_EQ(future.Get(),
            "Skill Title: Submit Form\n1. Click the Submit button.");
}

TEST_F(RecordReplayMesClientTest,
       GenerateSkillFromRecording_CapsAttachedScreenshots) {
  static constexpr size_t kTotalActions =
      RecordReplayMesClient::kMaxAttachedScreenshots + 2;
  ExpectExecuteModelAndRespond(
      "Skill Title: Multi-step Skill",
      base::BindOnce(
          [](const optimization_guide::proto::ContextHubRequest& request,
             const optimization_guide::ModelExecutionOptions& options) {
            EXPECT_EQ(base::checked_cast<size_t>(request.entry_items_size()),
                      RecordReplayMesClient::kMaxAttachedScreenshots);
            EXPECT_THAT(
                request.user_command(),
                HasSubstr("Action " + base::NumberToString(kTotalActions)));
          }));

  const std::string valid_jpeg = CreateTestJpegBytes();
  RecordingArtifacts artifacts;
  for (size_t i = 0; i < kTotalActions; ++i) {
    artifacts.actions.emplace_back(SkillRecordingActionType::kLeftClick,
                                   "Action " + base::NumberToString(i + 1),
                                   base::Milliseconds((i + 1) * 100));
    artifacts.actions.back().screenshot_jpeg_bytes = valid_jpeg;
  }

  TestFuture<std::optional<std::string>> future;
  client_.GenerateSkillFromRecording(artifacts, future.GetCallback());
  EXPECT_EQ(future.Get(), "Skill Title: Multi-step Skill");
}

TEST_F(RecordReplayMesClientTest,
       GenerateSkillFromRecording_EmptyInputAndExecutionFailures) {
  RecordingArtifacts empty_artifacts;
  TestFuture<std::optional<std::string>> empty_future;
  client_.GenerateSkillFromRecording(empty_artifacts,
                                     empty_future.GetCallback());
  EXPECT_FALSE(empty_future.IsReady());
  EXPECT_FALSE(empty_future.Get().has_value());

  EXPECT_CALL(
      mock_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce(WithArg<3>([](optimization_guide::
                                  OptimizationGuideModelExecutionResultCallback
                                      callback) {
        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::unexpected(
                    optimization_guide::OptimizationGuideModelExecutionError::
                        FromModelExecutionError(
                            optimization_guide::
                                OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure)),
                nullptr),
            nullptr);
      }))
      .WillOnce(WithArg<3>(
          [](optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            std::move(callback).Run(CreateSkillGenerationMesResult("   \n  "),
                                    nullptr);
          }))
      .WillOnce(WithArg<3>(
          [](optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::proto::Any invalid_any;
            invalid_any.set_type_url("type.googleapis.com/InvalidProto");
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    base::ok(std::move(invalid_any)), nullptr),
                nullptr);
          }));

  RecordingArtifacts artifacts;
  artifacts.actions.emplace_back(SkillRecordingActionType::kLeftClick,
                                 "Clicked Submit", base::Milliseconds(150));

  for (int i = 0; i < 3; ++i) {
    TestFuture<std::optional<std::string>> future;
    client_.GenerateSkillFromRecording(artifacts, future.GetCallback());
    EXPECT_FALSE(future.Get().has_value());
  }
}

TEST_F(RecordReplayMesClientTest,
       GenerateSkillFromRecording_ConcurrencyAndCancellation) {
  TestFuture<optimization_guide::OptimizationGuideModelExecutionResultCallback>
      execute_called_future;
  EXPECT_CALL(
      mock_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce(WithArg<3>(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            execute_called_future.SetValue(std::move(callback));
          }));

  RecordingArtifacts artifacts;
  artifacts.actions.emplace_back(SkillRecordingActionType::kLeftClick,
                                 "Clicked Submit", base::Milliseconds(150));

  TestFuture<std::optional<std::string>> first_future;
  client_.GenerateSkillFromRecording(artifacts, first_future.GetCallback());

  TestFuture<std::optional<std::string>> duplicate_future;
  client_.GenerateSkillFromRecording(artifacts, duplicate_future.GetCallback());
  EXPECT_FALSE(duplicate_future.IsReady());
  EXPECT_FALSE(duplicate_future.Get().has_value());

  optimization_guide::OptimizationGuideModelExecutionResultCallback
      captured_callback = execute_called_future.Take();
  EXPECT_FALSE(first_future.IsReady());

  client_.CancelPendingRequest();
  EXPECT_FALSE(first_future.Get().has_value());

  std::move(captured_callback)
      .Run(CreateSkillGenerationMesResult("Stale response"), nullptr);

  TestFuture<optimization_guide::OptimizationGuideModelExecutionResultCallback>
      destroy_execute_future;
  EXPECT_CALL(
      mock_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce(WithArg<3>(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            destroy_execute_future.SetValue(std::move(callback));
          }));

  auto transient_client =
      std::make_unique<RecordReplayMesClient>(&mock_model_executor_);
  TestFuture<std::optional<std::string>> destroyed_future;
  transient_client->GenerateSkillFromRecording(artifacts,
                                               destroyed_future.GetCallback());
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      held_callback = destroy_execute_future.Take();
  transient_client.reset();
  EXPECT_FALSE(destroyed_future.Get().has_value());
  std::move(held_callback)
      .Run(CreateSkillGenerationMesResult("Stale response"), nullptr);
}

}  // namespace
}  // namespace context_hub
