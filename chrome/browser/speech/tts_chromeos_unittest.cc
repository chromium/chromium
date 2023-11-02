// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the TTS platform implementation in Chrome OS.

#include "chrome/browser/speech/tts_chromeos.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/tts_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

class TtsChromeosTest : public testing::Test {
 public:
  TtsPlatformImplChromeOs* impl() { return &impl_; }

 private:
  TtsPlatformImplChromeOs impl_;
};

TEST_F(TtsChromeosTest, TestGetVoices) {
  TtsPlatformImplChromeOs* tts_chromeos =
      TtsPlatformImplChromeOs::GetInstance();

  // ARC++ (which supplies the voices for CHrome OS's tts platform), is
  // unavailable here.
  EXPECT_FALSE(tts_chromeos->PlatformImplSupported());

  // Returns true to not interfere with tts controller queueing.
  EXPECT_TRUE(tts_chromeos->PlatformImplInitialized());

  std::unique_ptr<std::vector<content::VoiceData>> voices =
      std::make_unique<std::vector<content::VoiceData>>();
  tts_chromeos->GetVoices(voices.get());

  EXPECT_TRUE(voices->empty());
}

TEST_F(TtsChromeosTest, PrefersGoogleTtsVoices) {
  std::vector<content::VoiceData> voices(3);
  voices[0].name = "google";
  voices[0].engine_id = extension_misc::kGoogleSpeechSynthesisExtensionId;
  voices[1].name = "Platform";
  voices[1].native = true;
  voices[2].name = "Espeak";
  voices[2].engine_id = extension_misc::kEspeakSpeechSynthesisExtensionId;

  impl()->FinalizeVoiceOrdering(voices);
  ASSERT_EQ(3U, voices.size());
  EXPECT_EQ(extension_misc::kGoogleSpeechSynthesisExtensionId,
            voices[0].engine_id);
  EXPECT_TRUE(voices[1].native);
  EXPECT_EQ(extension_misc::kEspeakSpeechSynthesisExtensionId,
            voices[2].engine_id);

  // Swap Google with Platform.
  std::iter_swap(voices.begin(), voices.begin() + 1);

  // Finalize it again.
  impl()->FinalizeVoiceOrdering(voices);

  // Back to original ordering.
  EXPECT_EQ(extension_misc::kGoogleSpeechSynthesisExtensionId,
            voices[0].engine_id);
  EXPECT_TRUE(voices[1].native);
  EXPECT_EQ(extension_misc::kEspeakSpeechSynthesisExtensionId,
            voices[2].engine_id);

  // Swap Google with Espeak.
  std::iter_swap(voices.begin(), voices.begin() + 2);

  // Finalize it again.
  impl()->FinalizeVoiceOrdering(voices);

  // Back to original ordering.
  EXPECT_EQ(extension_misc::kGoogleSpeechSynthesisExtensionId,
            voices[0].engine_id);
  EXPECT_TRUE(voices[1].native);
  EXPECT_EQ(extension_misc::kEspeakSpeechSynthesisExtensionId,
            voices[2].engine_id);

  // Rotate to get Platform, Espeak, Google.
  std::rotate(voices.begin(), voices.begin() + 1, voices.end());

  // Finalize it again.
  impl()->FinalizeVoiceOrdering(voices);

  // Back to original ordering.
  EXPECT_EQ(extension_misc::kGoogleSpeechSynthesisExtensionId,
            voices[0].engine_id);
  EXPECT_TRUE(voices[1].native);
  EXPECT_EQ(extension_misc::kEspeakSpeechSynthesisExtensionId,
            voices[2].engine_id);
}
