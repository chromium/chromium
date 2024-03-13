// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/speech/speech_recognition_private_recognizer.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_base_test.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "content/public/test/fake_speech_recognition_manager.h"

namespace {
const char kEnglishLocale[] = "en-US";
const char kFrenchLocale[] = "fr-FR";
}  // namespace

namespace extensions {

class FakeSpeechRecognitionPrivateDelegate
    : public SpeechRecognitionPrivateDelegate {
 public:
  void HandleSpeechRecognitionStopped(const std::string& key) override {
    handled_stop_ = true;
  }

  void HandleSpeechRecognitionResult(const std::string& key,
                                     const std::u16string& transcript,
                                     bool is_final) override {
    last_transcript_ = transcript;
    last_is_final_ = is_final;
  }

  void HandleSpeechRecognitionError(const std::string& key,
                                    const std::string& error) override {
    last_error_ = error;
  }

 private:
  friend class SpeechRecognitionPrivateRecognizerTest;

  bool handled_stop_ = false;
  std::u16string last_transcript_;
  bool last_is_final_ = false;
  std::string last_error_;
};

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
    SpeechRecognitionPrivateBaseTest::SetUpOnMainThread();
    delegate_ = std::make_unique<FakeSpeechRecognitionPrivateDelegate>();
    recognizer_ = std::make_unique<SpeechRecognitionPrivateRecognizer>(
        delegate_.get(), profile(), "example_id");
  }

  void TearDownOnMainThread() override {
    recognizer_.reset();
    delegate_.reset();
    SpeechRecognitionPrivateBaseTest::TearDownOnMainThread();
  }

  void HandleStart(std::optional<std::string> locale,
                   std::optional<bool> interim_results) {
    // In some cases, speech recognition will not be started e.g. if
    // HandleStart() is called when speech recognition is already active. In
    // these cases, we don't want to wait for speech recognition to start.
    recognizer_->HandleStart(
        locale, interim_results,
        base::BindOnce(&SpeechRecognitionPrivateRecognizerTest::OnStartCallback,
                       base::Unretained(this)));
  }

  void HandleStartAndWait(std::optional<std::string> locale,
                          std::optional<bool> interim_results) {
    recognizer_->HandleStart(
        locale, interim_results,
        base::BindOnce(&SpeechRecognitionPrivateRecognizerTest::OnStartCallback,
                       base::Unretained(this)));

    WaitForRecognitionStarted();

    // Make assertions.
    speech::SpeechRecognitionType expected_type = GetParam();
    ASSERT_EQ(expected_type, type());
    ASSERT_TRUE(ran_on_start_callback());
    ASSERT_EQ(SPEECH_RECOGNIZER_RECOGNIZING, recognizer()->current_state());
  }

  void HandleStopAndWait() {
    recognizer_->HandleStop(base::BindOnce(
        &SpeechRecognitionPrivateRecognizerTest::OnStopOnceCallback,
        base::Unretained(this)));

    WaitForRecognitionStopped();

    // Make assertions.
    ASSERT_TRUE(ran_on_stop_once_callback());
    ASSERT_EQ(SPEECH_RECOGNIZER_OFF, recognizer()->current_state());
  }

  void MaybeUpdateProperties(std::optional<std::string> locale,
                             std::optional<bool> interim_results) {
    recognizer_->MaybeUpdateProperties(
        locale, interim_results,
        base::BindOnce(&SpeechRecognitionPrivateRecognizerTest::OnStartCallback,
                       base::Unretained(this)));
  }

  void FakeSpeechRecognitionStateChanged(SpeechRecognizerStatus new_state) {
    recognizer_->OnSpeechRecognitionStateChanged(new_state);
  }

  void SendInterimFakeSpeechResult(const std::u16string& transcript) {
    recognizer_->OnSpeechResult(transcript, false, std::nullopt);
  }

  void OnStartCallback(speech::SpeechRecognitionType type,
                       std::optional<std::string> error) {
    type_ = type;
    on_start_callback_error_ = error.has_value() ? error.value() : "";
    ran_on_start_callback_ = true;
  }

  void OnStopOnceCallback(std::optional<std::string> error) {
    if (error.has_value())
      on_stop_once_callback_error_ = error.value();
    else
      on_stop_once_callback_error_ = "";

    ran_on_stop_once_callback_ = true;
  }

  SpeechRecognitionPrivateRecognizer* recognizer() { return recognizer_.get(); }

  speech::SpeechRecognitionType type() { return type_; }
  bool ran_on_start_callback() { return ran_on_start_callback_; }
  void set_ran_on_start_callback(bool value) { ran_on_start_callback_ = value; }

  bool ran_on_stop_once_callback() { return ran_on_stop_once_callback_; }
  void set_ran_on_stop_once_callback(bool value) {
    ran_on_stop_once_callback_ = value;
  }

  bool delegate_handled_stop() { return delegate_->handled_stop_; }
  void set_delegate_handled_stop(bool value) {
    delegate_->handled_stop_ = value;
  }

  std::string on_start_callback_error() { return on_start_callback_error_; }

  std::string on_stop_once_callback_error() {
    return on_stop_once_callback_error_;
  }

  std::u16string last_transcript() { return delegate_->last_transcript_; }
  bool last_is_final() { return delegate_->last_is_final_; }
  std::string last_error() { return delegate_->last_error_; }

  speech::SpeechRecognitionType type_ = speech::SpeechRecognitionType::kNetwork;
  bool ran_on_start_callback_ = false;
  bool ran_on_stop_once_callback_ = false;
  std::string on_start_callback_error_;
  std::string on_stop_once_callback_error_;

  std::unique_ptr<SpeechRecognitionPrivateRecognizer> recognizer_;
  std::unique_ptr<FakeSpeechRecognitionPrivateDelegate> delegate_;
};

INSTANTIATE_TEST_SUITE_P(
    Network,
    SpeechRecognitionPrivateRecognizerTest,
    ::testing::Values(speech::SpeechRecognitionType::kNetwork));

INSTANTIATE_TEST_SUITE_P(
    OnDevice,
    SpeechRecognitionPrivateRecognizerTest,
    ::testing::Values(speech::SpeechRecognitionType::kOnDevice));

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateRecognizerTest,
                       MaybeUpdateProperties) {
  std::optional<std::string> locale;
  std::optional<bool> interim_results;
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
  std::optional<std::string> locale;
  std::optional<bool> interim_results;
  HandleStartAndWait(locale, interim_results);
}

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateRecognizerTest,
                       RecognitionStartsAndStops) {
  std::optional<std::string> locale;
  std::optional<bool> interim_results;

  // Start speech recognition.
  HandleStartAndWait(locale, interim_results);

  // Stop speech recognition.
  HandleStopAndWait();
  ASSERT_TRUE(delegate_handled_stop());
}

// Tests how HandleStart() behaves if speech recognition is already active. It
// should run the OnceCallback with an error.
IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateRecognizerTest,
                       HandleStartAlreadyStarted) {
  std::optional<std::string> locale;
  std::optional<bool> interim_results;
  HandleStartAndWait(locale, interim_results);
  ASSERT_EQ("", on_start_callback_error());
  ASSERT_EQ(kEnglishLocale, recognizer()->locale());
  ASSERT_FALSE(recognizer()->interim_results());

  // Try to update properties and start the recognizer again. This should
  // cause an error because speech recognition is already active. Properties
  // should not be updated and an error should be returned.
  interim_results = true;
  set_ran_on_start_callback(false);
  HandleStart(locale, interim_results);
  ASSERT_TRUE(ran_on_start_callback());
  ASSERT_EQ("Speech recognition already started", on_start_callback_error());
  ASSERT_EQ(SPEECH_RECOGNIZER_OFF, recognizer()->current_state());
  ASSERT_EQ(kEnglishLocale, recognizer()->locale());
  ASSERT_FALSE(recognizer()->interim_results());
}

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateRecognizerTest,
                       RecognitionStartsAndStopsTwice) {
  std::optional<std::string> locale;
  std::optional<bool> interim_results;
  HandleStartAndWait(locale, interim_results);
  ASSERT_EQ(kEnglishLocale, recognizer()->locale());
  ASSERT_EQ(false, recognizer()->interim_results());

  HandleStopAndWait();
  ASSERT_TRUE(delegate_handled_stop());
  ASSERT_EQ(kEnglishLocale, recognizer()->locale());
  ASSERT_EQ(false, recognizer()->interim_results());

  // Update properties and start the recognizer again.
  // Keep the locale as en-US, otherwise the the on-device variant of this
  // test will fail because on-device speech recognition is only supported in
  // en-US.
  interim_results = true;
  set_ran_on_start_callback(false);
  set_ran_on_stop_once_callback(false);
  set_delegate_handled_stop(false);
  HandleStartAndWait(locale, interim_results);
  ASSERT_EQ(kEnglishLocale, recognizer()->locale());
  ASSERT_EQ(true, recognizer()->interim_results());

  HandleStopAndWait();
  ASSERT_TRUE(delegate_handled_stop());
  ASSERT_EQ(kEnglishLocale, recognizer()->locale());
  ASSERT_EQ(true, recognizer()->interim_results());
}

// Tests how HandleStop() behaves if speech recognition is already off. It
// should run the OnceCallback with an error, but should not run the
// RepeatingCallback.
IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateRecognizerTest,
                       HandleStopNeverStarted) {
  HandleStopAndWait();
  ASSERT_EQ("Speech recognition already stopped",
            on_stop_once_callback_error());
  ASSERT_FALSE(delegate_handled_stop());
  ASSERT_EQ(kEnglishLocale, recognizer()->locale());
  ASSERT_EQ(false, recognizer()->interim_results());
}

// Tests that we run the correct callback when speech recognition is stopped in
// the background.
IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateRecognizerTest,
                       StoppedInBackground) {
  HandleStartAndWait(std::optional<std::string>(), std::optional<bool>());
  FakeSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_READY);
  ASSERT_TRUE(delegate_handled_stop());
  ASSERT_FALSE(ran_on_stop_once_callback());
  ASSERT_EQ("", on_stop_once_callback_error());
  ASSERT_EQ(SPEECH_RECOGNIZER_OFF, recognizer()->current_state());
}

// Tests that we run the correct callbacks when speech recognition encounters
// an error.
IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateRecognizerTest, Error) {
  HandleStartAndWait(std::optional<std::string>(), std::optional<bool>());
  SendErrorAndWait();
  ASSERT_TRUE(delegate_handled_stop());
  ASSERT_FALSE(ran_on_stop_once_callback());
  ASSERT_EQ(SPEECH_RECOGNIZER_OFF, recognizer()->current_state());
  ASSERT_EQ("A speech recognition error occurred", last_error());
}

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateRecognizerTest, OnSpeechResult) {
  HandleStartAndWait(std::optional<std::string>(), std::optional<bool>());

  // Set interim_results to false. This means we should only respond to final
  // speech recognition results.
  MaybeUpdateProperties(std::optional<std::string>(),
                        std::optional<bool>(false));
  SendInterimFakeSpeechResult(u"Interim result");
  ASSERT_EQ(u"", last_transcript());
  ASSERT_FALSE(last_is_final());

  SendFinalResultAndWait("Final result");
  ASSERT_EQ(u"Final result", last_transcript());
  ASSERT_TRUE(last_is_final());

  // Set interim_results to true. This means we should respond to both final
  // and interim speech recognition results.
  MaybeUpdateProperties(std::optional<std::string>(),
                        std::optional<bool>(true));
  SendInterimFakeSpeechResult(u"Interim result");
  ASSERT_EQ(u"Interim result", last_transcript());
  ASSERT_FALSE(last_is_final());

  SendFinalResultAndWait("Final result");
  ASSERT_EQ(u"Final result", last_transcript());
  ASSERT_TRUE(last_is_final());
}

// Verifies that the speech recognizer can handle transitions between states.
// Some of the below states are intentionally erroneous to ensure the recognizer
// can handle unexpected input.
IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateRecognizerTest, SetState) {
  FakeSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_READY);
  FakeSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_RECOGNIZING);
  FakeSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_ERROR);
  // Erroneously change the state to 'recognizing'. The recognizer should
  // intelligently handle this case.
  FakeSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_RECOGNIZING);
  ASSERT_EQ(SPEECH_RECOGNIZER_OFF, recognizer()->current_state());
}

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateRecognizerTest,
                       StopWhenNeverStarted) {
  std::optional<std::string> locale;
  std::optional<bool> interim_results;
  // Attempt to start speech recognition. Don't wait for `on_start_callback` to
  // be run.
  HandleStart(locale, interim_results);
  ASSERT_FALSE(ran_on_start_callback());
  // Immediately turn speech recognition off.
  HandleStopAndWait();
  // Ensure there are no dangling callbacks e.g. that both `on_start_callback`
  // and `on_stop_callback` should be run.
  ASSERT_TRUE(ran_on_start_callback());
  ASSERT_TRUE(ran_on_stop_once_callback());
}

}  // namespace extensions
