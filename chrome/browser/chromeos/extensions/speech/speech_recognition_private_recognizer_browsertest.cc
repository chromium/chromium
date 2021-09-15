// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_recognizer.h"

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_base_test.h"

namespace {
const char kEnglishLocale[] = "en-US";
const char kFrenchLocale[] = "fr-FR";
}  // namespace

namespace extensions {

class SpeechRecognitionPrivateRecognizerTest
    : public SpeechRecognitionPrivateBaseTest {
 protected:
  SpeechRecognitionPrivateRecognizerTest() {}
  ~SpeechRecognitionPrivateRecognizerTest() override = default;
  SpeechRecognitionPrivateRecognizerTest(
      const SpeechRecognitionPrivateRecognizerTest&) = delete;
  SpeechRecognitionPrivateRecognizerTest& operator=(
      const SpeechRecognitionPrivateRecognizerTest&) = delete;

  void SetUpOnMainThread() override {
    recognizer_ = std::make_unique<SpeechRecognitionPrivateRecognizer>();
    SpeechRecognitionPrivateBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    recognizer_.reset();
    SpeechRecognitionPrivateBaseTest::TearDownOnMainThread();
  }

  void HandleStartAndWait(absl::optional<std::string> locale,
                          absl::optional<bool> interim_results) {
    recognizer_->HandleStart(
        locale, interim_results,
        base::BindOnce(&SpeechRecognitionPrivateRecognizerTest::OnStartCallback,
                       base::Unretained(this)));
    SpeechRecognitionPrivateBaseTest::WaitForRecognitionStarted();
  }

  void MaybeUpdateProperties(absl::optional<std::string> locale,
                             absl::optional<bool> interim_results) {
    recognizer_->MaybeUpdateProperties(
        locale, interim_results,
        base::BindOnce(&SpeechRecognitionPrivateRecognizerTest::OnStartCallback,
                       base::Unretained(this)));
  }

  void OnStartCallback() { ran_on_start_callback_ = true; }
  bool ran_on_start_callback() { return ran_on_start_callback_; }
  void set_ran_on_start_callback(bool value) { ran_on_start_callback_ = value; }
  SpeechRecognitionPrivateRecognizer* recognizer() { return recognizer_.get(); }

  bool ran_on_start_callback_ = false;
  std::unique_ptr<SpeechRecognitionPrivateRecognizer> recognizer_;
};

INSTANTIATE_TEST_SUITE_P(Network,
                         SpeechRecognitionPrivateRecognizerTest,
                         ::testing::Values(kNetworkRecognition));

INSTANTIATE_TEST_SUITE_P(OnDevice,
                         SpeechRecognitionPrivateRecognizerTest,
                         ::testing::Values(kOnDeviceRecognition));

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateRecognizerTest,
                       MaybeUpdateProperties) {
  absl::optional<std::string> locale;
  absl::optional<bool> interim_results;
  MaybeUpdateProperties(locale, interim_results);
  ASSERT_EQ(kEnglishLocale, recognizer()->locale());
  ASSERT_FALSE(recognizer()->interim_results());

  locale = kFrenchLocale;
  MaybeUpdateProperties(locale, interim_results);
  ASSERT_EQ(kFrenchLocale, recognizer()->locale());
  ASSERT_FALSE(recognizer()->interim_results());

  interim_results = true;
  MaybeUpdateProperties(locale, interim_results);
  ASSERT_EQ(kFrenchLocale, recognizer()->locale());
  ASSERT_TRUE(recognizer()->interim_results());

  locale = kEnglishLocale;
  MaybeUpdateProperties(locale, interim_results);
  ASSERT_EQ(kEnglishLocale, recognizer()->locale());
  ASSERT_TRUE(recognizer()->interim_results());
}

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateRecognizerTest,
                       RecognitionStarts) {
  absl::optional<std::string> locale;
  absl::optional<bool> interim_results;
  HandleStartAndWait(locale, interim_results);
  ASSERT_EQ(SPEECH_RECOGNIZER_RECOGNIZING, recognizer()->current_state());
  ASSERT_TRUE(ran_on_start_callback());
}

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateRecognizerTest,
                       RecognitionStartsTwice) {
  absl::optional<std::string> locale;
  absl::optional<bool> interim_results;
  HandleStartAndWait(locale, interim_results);
  ASSERT_TRUE(ran_on_start_callback());
  ASSERT_EQ(SPEECH_RECOGNIZER_RECOGNIZING, recognizer()->current_state());
  ASSERT_EQ(kEnglishLocale, recognizer()->locale());
  ASSERT_FALSE(recognizer()->interim_results());

  // Update properties and start the recognizer again.
  // Keep the locale as en-US, otherwise the the on-device variant of this
  // test will fail because on-device speech recognition is only supported in
  // en-US.
  interim_results = true;
  set_ran_on_start_callback(false);
  HandleStartAndWait(locale, interim_results);
  ASSERT_TRUE(ran_on_start_callback());
  ASSERT_EQ(SPEECH_RECOGNIZER_RECOGNIZING, recognizer()->current_state());
  ASSERT_EQ(kEnglishLocale, recognizer()->locale());
  ASSERT_TRUE(recognizer()->interim_results());
}

}  // namespace extensions
