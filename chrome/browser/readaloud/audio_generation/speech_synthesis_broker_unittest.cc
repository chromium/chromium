// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/audio_generation/speech_synthesis_broker.h"

#include <string_view>

#include "base/test/bind.h"
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
            std::string_view result_bytes(
                reinterpret_cast<const char*>(response_bytes.data()),
                response_bytes.size());
            EXPECT_EQ(result_bytes, "fake_serialized_proto_bytes_12345");
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

}  // namespace readaloud
