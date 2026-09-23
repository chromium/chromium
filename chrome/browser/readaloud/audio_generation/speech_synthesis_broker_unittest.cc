// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/audio_generation/speech_synthesis_broker.h"

#include <string_view>

#include "base/strings/string_view_util.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

class SpeechSynthesisBrokerTest : public ::testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  SpeechSynthesisBroker broker_;
};

TEST_F(SpeechSynthesisBrokerTest,
       BuildSynthesizeRequestDefaultsAndCustomConfig) {
  // 1. Verify default voice and language configuration in proto output.
  optimization_guide::proto::ReadAloudSynthesizeRequest default_request =
      broker_.BuildSynthesizeRequest(u"Hello world");
  EXPECT_EQ(default_request.text_chunk(), "Hello world");
  EXPECT_EQ(default_request.voice_id(), "msf00006");
  EXPECT_EQ(default_request.language_code(), "en");

  // 2. Verify custom voice and language configuration.
  broker_.SetVoice("es-ES-Wavenet-B");
  broker_.SetLanguageCode("es");
  optimization_guide::proto::ReadAloudSynthesizeRequest custom_request =
      broker_.BuildSynthesizeRequest(u"Hola mundo");
  EXPECT_EQ(custom_request.text_chunk(), "Hola mundo");
  EXPECT_EQ(custom_request.voice_id(), "es-ES-Wavenet-B");
  EXPECT_EQ(custom_request.language_code(), "es");

  // 3. Verify fallback to defaults when empty values are provided.
  broker_.SetVoice("");
  broker_.SetLanguageCode("");
  optimization_guide::proto::ReadAloudSynthesizeRequest fallback_request =
      broker_.BuildSynthesizeRequest(u"Hello again");
  EXPECT_EQ(fallback_request.voice_id(), "msf00006");
  EXPECT_EQ(fallback_request.language_code(), "en");
}

TEST_F(SpeechSynthesisBrokerTest, BuildSynthesizeRequestUtf8Conversion) {
  optimization_guide::proto::ReadAloudSynthesizeRequest request =
      broker_.BuildSynthesizeRequest(u"Bonjour le monde! こんにちは 123");
  EXPECT_EQ(request.text_chunk(), "Bonjour le monde! こんにちは 123");
}

TEST_F(SpeechSynthesisBrokerTest, BuildSynthesizeRequestEmptyTextChunk) {
  optimization_guide::proto::ReadAloudSynthesizeRequest request =
      broker_.BuildSynthesizeRequest(u"");
  EXPECT_TRUE(request.text_chunk().empty());
  EXPECT_EQ(request.voice_id(), "msf00006");
  EXPECT_EQ(request.language_code(), "en");
}

TEST_F(SpeechSynthesisBrokerTest, SetLanguageCodeValidTags) {
  broker_.SetLanguageCode("es-ES");
  EXPECT_EQ(broker_.language_code(), "es-ES");
  EXPECT_EQ(broker_.language_tag().tag_string(), "es-ES");

  broker_.SetLanguageCode("fr");
  EXPECT_EQ(broker_.language_code(), "fr");
  EXPECT_EQ(broker_.language_tag().tag_string(), "fr");
}

TEST_F(SpeechSynthesisBrokerTest, SetLanguageCodeEmptyStringFallback) {
  broker_.SetLanguageCode("es");
  EXPECT_EQ(broker_.language_code(), "es");

  // Reset with empty string; should fall back to default language tag.
  broker_.SetLanguageCode("");
  EXPECT_EQ(broker_.language_code(), "en");
}

TEST_F(SpeechSynthesisBrokerTest, SetLanguageCodeInvalidTagFallback) {
  broker_.SetLanguageCode("invalid_xyz_tag_123");
  // Invalid language tags should be rejected safely and fall back to default.
  EXPECT_EQ(broker_.language_code(), "en");
}

TEST_F(SpeechSynthesisBrokerTest, SynthesizeSpeechNullOptGuideService) {
  bool callback_called = false;
  broker_.SynthesizeSpeech(
      /*opt_guide_service=*/nullptr, u"Hello world",
      base::BindLambdaForTesting(
          [&](mojo_base::BigBuffer response_bytes, bool success) {
            callback_called = true;
            EXPECT_FALSE(success);
            EXPECT_EQ(response_bytes.size(), 0u);
          }));
  EXPECT_TRUE(callback_called);
}

TEST_F(SpeechSynthesisBrokerTest, SynthesizeSpeechEmptyTextChunk) {
  testing::NiceMock<MockOptimizationGuideKeyedService> mock_opt_guide;
  bool callback_called = false;
  broker_.SynthesizeSpeech(
      &mock_opt_guide, u"",
      base::BindLambdaForTesting(
          [&](mojo_base::BigBuffer response_bytes, bool success) {
            callback_called = true;
            EXPECT_FALSE(success);
            EXPECT_EQ(response_bytes.size(), 0u);
          }));
  EXPECT_TRUE(callback_called);
}

TEST_F(SpeechSynthesisBrokerTest, SynthesizeSpeechModelExecutionSuccess) {
  testing::NiceMock<MockOptimizationGuideKeyedService> mock_opt_guide;

  std::string fake_response_payload = "fake_serialized_proto_bytes_12345";
  optimization_guide::proto::Any any;
  any.set_value(fake_response_payload);

  EXPECT_CALL(
      mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudSynthesize,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&any](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result(
                any, /*execution_info=*/nullptr);
            std::move(callback).Run(std::move(result), /*log_entry=*/nullptr);
          });

  bool callback_called = false;
  broker_.SynthesizeSpeech(
      &mock_opt_guide, u"Hello world",
      base::BindLambdaForTesting(
          [&](mojo_base::BigBuffer response_bytes, bool success) {
            callback_called = true;
            EXPECT_TRUE(success);
            EXPECT_EQ(base::as_string_view(response_bytes),
                      "fake_serialized_proto_bytes_12345");
          }));
  EXPECT_TRUE(callback_called);
}

TEST_F(SpeechSynthesisBrokerTest, SynthesizeSpeechModelExecutionFailure) {
  testing::NiceMock<MockOptimizationGuideKeyedService> mock_opt_guide;

  EXPECT_CALL(
      mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudSynthesize,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result(
                base::unexpected(
                    optimization_guide::OptimizationGuideModelExecutionError::
                        FromModelExecutionError(
                            optimization_guide::
                                OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure)),
                /*execution_info=*/nullptr);
            std::move(callback).Run(std::move(result), /*log_entry=*/nullptr);
          });

  bool callback_called = false;
  broker_.SynthesizeSpeech(
      &mock_opt_guide, u"Hello world",
      base::BindLambdaForTesting(
          [&](mojo_base::BigBuffer response_bytes, bool success) {
            callback_called = true;
            EXPECT_FALSE(success);
            EXPECT_EQ(response_bytes.size(), 0u);
          }));
  EXPECT_TRUE(callback_called);
}

TEST_F(SpeechSynthesisBrokerTest, BuildSynthesizeRequestWithVoiceOverride) {
  optimization_guide::proto::ReadAloudSynthesizeRequest req1 =
      broker_.BuildSynthesizeRequest(
          u"Speaker 1 chunk", SpeechSynthesisBroker::kOverviewVoiceSpeaker1);
  EXPECT_EQ(req1.voice_id(), "msf00006");

  optimization_guide::proto::ReadAloudSynthesizeRequest req2 =
      broker_.BuildSynthesizeRequest(
          u"Speaker 2 chunk", SpeechSynthesisBroker::kOverviewVoiceSpeaker2);
  EXPECT_EQ(req2.voice_id(), "msm00013");
}

TEST_F(SpeechSynthesisBrokerTest, DialogueVoiceConstantsOppositeSex) {
  EXPECT_STRNE(SpeechSynthesisBroker::kOverviewVoiceSpeaker1,
               SpeechSynthesisBroker::kOverviewVoiceSpeaker2);
  EXPECT_THAT(SpeechSynthesisBroker::kOverviewVoiceSpeaker1,
              testing::StartsWith("msf"));
  EXPECT_THAT(SpeechSynthesisBroker::kOverviewVoiceSpeaker2,
              testing::StartsWith("msm"));
}

TEST_F(SpeechSynthesisBrokerTest,
       SynthesizeSpeechWithVoiceOverrideForwardsToExecuteModel) {
  testing::NiceMock<MockOptimizationGuideKeyedService> mock_opt_guide;

  optimization_guide::proto::Any any;
  any.set_value("audio_bytes_speaker2");

  EXPECT_CALL(
      mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudSynthesize,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&any](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            const auto& synthesize_request =
                static_cast<const optimization_guide::proto::
                                ReadAloudSynthesizeRequest&>(request_metadata);
            EXPECT_EQ(synthesize_request.voice_id(),
                      SpeechSynthesisBroker::kOverviewVoiceSpeaker2);
            EXPECT_EQ(synthesize_request.text_chunk(),
                      "Speaker 2 dialogue line");

            optimization_guide::OptimizationGuideModelExecutionResult result(
                any, /*execution_info=*/nullptr);
            std::move(callback).Run(std::move(result), /*log_entry=*/nullptr);
          });

  base::test::TestFuture<mojo_base::BigBuffer, bool> future;
  broker_.SynthesizeSpeech(&mock_opt_guide, u"Speaker 2 dialogue line",
                           SpeechSynthesisBroker::kOverviewVoiceSpeaker2,
                           future.GetCallback());
  auto [response_bytes, success] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_EQ(base::as_string_view(response_bytes), "audio_bytes_speaker2");
}

TEST_F(SpeechSynthesisBrokerTest,
       SynthesizeSpeechEmptyVoiceOverrideUsesConfiguredVoice) {
  testing::NiceMock<MockOptimizationGuideKeyedService> mock_opt_guide;
  broker_.SetVoice("custom-voice-id");

  optimization_guide::proto::Any any;
  any.set_value("audio_bytes_custom");

  EXPECT_CALL(
      mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudSynthesize,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&any](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            const auto& synthesize_request =
                static_cast<const optimization_guide::proto::
                                ReadAloudSynthesizeRequest&>(request_metadata);
            EXPECT_EQ(synthesize_request.voice_id(), "custom-voice-id");
            EXPECT_EQ(synthesize_request.text_chunk(), "Custom voice chunk");

            optimization_guide::OptimizationGuideModelExecutionResult result(
                any, /*execution_info=*/nullptr);
            std::move(callback).Run(std::move(result), /*log_entry=*/nullptr);
          });

  base::test::TestFuture<mojo_base::BigBuffer, bool> future;
  broker_.SynthesizeSpeech(&mock_opt_guide, u"Custom voice chunk",
                           /*voice_id_override=*/"", future.GetCallback());
  auto [response_bytes, success] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_EQ(base::as_string_view(response_bytes), "audio_bytes_custom");
}

}  // namespace readaloud
