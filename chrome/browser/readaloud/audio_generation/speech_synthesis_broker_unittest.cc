// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/audio_generation/speech_synthesis_broker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

class SpeechSynthesisBrokerTest : public ::testing::Test {
 protected:
  SpeechSynthesisBroker broker_;
};

TEST_F(SpeechSynthesisBrokerTest,
       BuildSynthesizeRequestDefaultsAndCustomConfig) {
  // 1. Verify default voice and language configuration in proto output.
  optimization_guide::proto::ReadAloudSynthesizeRequest default_request =
      broker_.BuildSynthesizeRequest(u"Hello world");
  EXPECT_EQ(default_request.text_chunk(), "Hello world");
  EXPECT_EQ(default_request.voice_id(), "en-US-Wavenet-A");
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
  EXPECT_EQ(fallback_request.voice_id(), "en-US-Wavenet-A");
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
  EXPECT_EQ(request.voice_id(), "en-US-Wavenet-A");
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

}  // namespace readaloud
