// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_manager.h"

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_base_test.h"
#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_recognizer.h"

namespace {
const char kEnglishLocale[] = "en-US";
}  // namespace

namespace extensions {

class SpeechRecognitionPrivateManagerTest
    : public SpeechRecognitionPrivateBaseTest {
 protected:
  SpeechRecognitionPrivateManagerTest() {}
  ~SpeechRecognitionPrivateManagerTest() override = default;
  SpeechRecognitionPrivateManagerTest(
      const SpeechRecognitionPrivateManagerTest&) = delete;
  SpeechRecognitionPrivateManagerTest& operator=(
      const SpeechRecognitionPrivateManagerTest&) = delete;

  void TearDownOnMainThread() override {
    SpeechRecogntionPrivateManager::GetInstance()->recognition_data_.clear();
    SpeechRecognitionPrivateBaseTest::TearDownOnMainThread();
  }

  std::string CreateKey(const std::string& extension_id,
                        absl::optional<int> client_id) {
    return SpeechRecogntionPrivateManager::GetInstance()->CreateKey(
        extension_id, client_id);
  }

  void HandleStartAndWait(const std::string& key,
                          absl::optional<std::string> locale,
                          absl::optional<bool> interim_results,
                          base::OnceClosure on_start_callback) {
    SpeechRecogntionPrivateManager::GetInstance()->HandleStart(
        key, locale, interim_results, std::move(on_start_callback));
    SpeechRecognitionPrivateBaseTest::WaitForRecognitionStarted();
  }

  SpeechRecognitionPrivateRecognizer* GetSpeechRecognizer(
      const std::string& key) {
    return SpeechRecogntionPrivateManager::GetInstance()->GetSpeechRecognizer(
        key);
  }
};

INSTANTIATE_TEST_SUITE_P(Network,
                         SpeechRecognitionPrivateManagerTest,
                         ::testing::Values(kNetworkRecognition));

INSTANTIATE_TEST_SUITE_P(OnDevice,
                         SpeechRecognitionPrivateManagerTest,
                         ::testing::Values(kOnDeviceRecognition));

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateManagerTest, CreateKey) {
  ASSERT_EQ("Testing", CreateKey("Testing", absl::optional<int>()));
  ASSERT_EQ("Testing.0", CreateKey("Testing", absl::optional<int>(0)));
  ASSERT_EQ("Testing.1", CreateKey("Testing", absl::optional<int>(1)));
}

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateManagerTest,
                       GetSpeechRecognizer) {
  SpeechRecognitionPrivateRecognizer* first_recognizer = nullptr;
  SpeechRecognitionPrivateRecognizer* second_recognizer = nullptr;
  first_recognizer = GetSpeechRecognizer("Testing");
  second_recognizer = GetSpeechRecognizer("Testing");
  ASSERT_NE(nullptr, first_recognizer);
  ASSERT_NE(nullptr, second_recognizer);
  ASSERT_EQ(first_recognizer, second_recognizer);
  second_recognizer = GetSpeechRecognizer("Testing.0");
  ASSERT_NE(nullptr, second_recognizer);
  ASSERT_NE(first_recognizer, second_recognizer);
}

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateManagerTest, HandleStart) {
  const std::string key = "Testing";
  absl::optional<std::string> locale;
  absl::optional<bool> interim_results(true);

  HandleStartAndWait(key, locale, interim_results, base::DoNothing());
  SpeechRecognitionPrivateRecognizer* first_recognizer =
      GetSpeechRecognizer(key);
  ASSERT_NE(nullptr, first_recognizer);
  ASSERT_EQ(kEnglishLocale, first_recognizer->locale());
  ASSERT_TRUE(first_recognizer->interim_results());
  ASSERT_EQ(SPEECH_RECOGNIZER_RECOGNIZING, first_recognizer->current_state());

  // Change some properties and start again.
  interim_results = false;
  HandleStartAndWait(key, locale, interim_results, base::DoNothing());
  SpeechRecognitionPrivateRecognizer* second_recognizer =
      GetSpeechRecognizer(key);
  ASSERT_NE(nullptr, second_recognizer);
  ASSERT_EQ(first_recognizer, second_recognizer);
  ASSERT_EQ(kEnglishLocale, second_recognizer->locale());
  ASSERT_FALSE(second_recognizer->interim_results());
  ASSERT_EQ(SPEECH_RECOGNIZER_RECOGNIZING, second_recognizer->current_state());
}

}  // namespace extensions
