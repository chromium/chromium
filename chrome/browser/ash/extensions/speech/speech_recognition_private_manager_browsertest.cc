// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/speech/speech_recognition_private_manager.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_base_test.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_manager_factory.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_recognizer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

namespace {
const char kEnglishLocale[] = "en-US";
const char kExtensionId[] = "egfdjlfmgnehecnclamagfafdccgfndp";
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

  void SetUpOnMainThread() override {
    SpeechRecognitionPrivateBaseTest::SetUpOnMainThread();
    manager_ =
        SpeechRecognitionPrivateManagerFactory::GetForBrowserContext(profile());
  }

  void TearDownOnMainThread() override {
    manager_->recognition_data_.clear();
    SpeechRecognitionPrivateBaseTest::TearDownOnMainThread();
  }

  std::string CreateKey(const std::string& extension_id,
                        std::optional<int> client_id) {
    return manager_->CreateKey(extension_id, client_id);
  }

  void HandleStart(
      const std::string& key,
      std::optional<std::string> locale,
      std::optional<bool> interim_results,
      base::OnceCallback<void(speech::SpeechRecognitionType,
                              std::optional<std::string>)> on_start_callback) {
    manager_->HandleStart(key, locale, interim_results,
                          std::move(on_start_callback));
  }

  void HandleStopAndWait(
      const std::string& key,
      base::OnceCallback<void(std::optional<std::string>)> callback) {
    manager_->HandleStop(key, std::move(callback));
    WaitForRecognitionStopped();
  }

  SpeechRecognitionPrivateRecognizer* GetSpeechRecognizer(
      const std::string& key) {
    return manager_->GetSpeechRecognizer(key);
  }

  void DispatchOnStopEvent(const std::string& key) {
    manager_->HandleSpeechRecognitionStopped(key);
  }

  void DispatchOnResultEvent(const std::string& key,
                             const std::u16string transcript,
                             bool is_final) {
    manager_->HandleSpeechRecognitionResult(key, transcript, is_final);
  }

  void DispatchOnErrorEvent(const std::string& key,
                            const std::string& message) {
    manager_->HandleSpeechRecognitionError(key, message);
  }

 private:
  raw_ptr<SpeechRecognitionPrivateManager, DanglingUntriaged> manager_;
};

INSTANTIATE_TEST_SUITE_P(
    Network,
    SpeechRecognitionPrivateManagerTest,
    ::testing::Values(speech::SpeechRecognitionType::kNetwork));

INSTANTIATE_TEST_SUITE_P(
    OnDevice,
    SpeechRecognitionPrivateManagerTest,
    ::testing::Values(speech::SpeechRecognitionType::kOnDevice));

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateManagerTest, CreateKey) {
  ASSERT_EQ("Testing", CreateKey("Testing", std::optional<int>()));
  ASSERT_EQ("Testing.0", CreateKey("Testing", std::optional<int>(0)));
  ASSERT_EQ("Testing.1", CreateKey("Testing", std::optional<int>(1)));
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
  std::optional<std::string> locale;
  std::optional<bool> interim_results(true);

  HandleStart(key, locale, interim_results, base::DoNothing());
  WaitForRecognitionStarted();
  SpeechRecognitionPrivateRecognizer* first_recognizer =
      GetSpeechRecognizer(key);
  ASSERT_NE(nullptr, first_recognizer);
  ASSERT_EQ(kEnglishLocale, first_recognizer->locale());
  ASSERT_TRUE(first_recognizer->interim_results());
  ASSERT_EQ(SPEECH_RECOGNIZER_RECOGNIZING, first_recognizer->current_state());

  // Try to change some properties and start again. Calling HandleStart() when
  // speech recognition is active should cause an error. The error message is
  // verified in SpeechRecognitionPrivateRecognizerTest. For this test, just
  // verify that properties are not updated and that speech recognition is
  // canceled.
  interim_results = false;
  HandleStart(key, locale, interim_results, base::DoNothing());
  SpeechRecognitionPrivateRecognizer* second_recognizer =
      GetSpeechRecognizer(key);
  ASSERT_NE(nullptr, second_recognizer);
  ASSERT_EQ(first_recognizer, second_recognizer);
  ASSERT_EQ(kEnglishLocale, second_recognizer->locale());
  ASSERT_TRUE(second_recognizer->interim_results());
  ASSERT_EQ(SPEECH_RECOGNIZER_OFF, second_recognizer->current_state());
}

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateManagerTest,
                       HandleStartAndStop) {
  const std::string key = "Testing";
  std::optional<std::string> locale;
  std::optional<bool> interim_results(true);

  HandleStart(key, locale, interim_results, base::DoNothing());
  WaitForRecognitionStarted();
  SpeechRecognitionPrivateRecognizer* recognizer = GetSpeechRecognizer(key);
  ASSERT_NE(nullptr, recognizer);
  ASSERT_EQ(SPEECH_RECOGNIZER_RECOGNIZING, recognizer->current_state());

  HandleStopAndWait(key, base::DoNothing());
  recognizer = GetSpeechRecognizer(key);
  ASSERT_NE(nullptr, recognizer);
  ASSERT_EQ(SPEECH_RECOGNIZER_OFF, recognizer->current_state());
}

// Tests that events can be dispatched from the SpeechRecognitionPrivateManager
// and received and processed in an extension.
IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateManagerTest,
                       DispatchOnStopEvent) {
  ASSERT_TRUE(RunSpeechRecognitionPrivateTest("onstop_event")) << message_;

  const char* kExtensionIdAndIncorrectClientId =
      "egfdjlfmgnehecnclamagfafdccgfndp.0";
  const char* kCorrectExtensionIdAndClientId =
      "egfdjlfmgnehecnclamagfafdccgfndp.4";
  const char* kSkippingEvent = "Skipping event";
  const char* kProcessingEvent = "Processing event";

  // Send onStop events and ensure that we only process the event whose client
  // ID matches the extension's client ID.
  const struct {
    const char* key;
    const char* expected;
  } kTestCases[] = {{kExtensionId, kSkippingEvent},
                    {kExtensionIdAndIncorrectClientId, kSkippingEvent},
                    {kCorrectExtensionIdAndClientId, kProcessingEvent}};

  for (const auto& test : kTestCases) {
    ExtensionTestMessageListener listener(test.expected);
    DispatchOnStopEvent(test.key);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }
}

// Tests that events can be dispatched from the SpeechRecognitionPrivateManager
// and received and processed in an extension.
IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateManagerTest,
                       DispatchOnResultEvent) {
  ASSERT_TRUE(RunSpeechRecognitionPrivateTest("onresult_event")) << message_;

  const char* kFirstClient = "egfdjlfmgnehecnclamagfafdccgfndp.1";
  const char* kSecondClient = "egfdjlfmgnehecnclamagfafdccgfndp.2";
  const char* kSuccess = "Received result";
  const char* kFirstClientSkip = "Skipping event in first listener";
  const char* kSecondClientSkip = "Skipping event in second listener";
  const std::u16string kTranscript = u"This is a test";

  const struct {
    const char* key;
    const std::u16string transcript;
    const bool is_final;
    const char* expected_success_message;
    const char* expected_skip_message;
  } kTestCases[] = {
      {kFirstClient, kTranscript, false, kSuccess, kSecondClientSkip},
      {kSecondClient, kTranscript, true, kSuccess, kFirstClientSkip}};

  for (const auto& test : kTestCases) {
    // For each onResult event, verify that it was successfully handled in one
    // listener and dropped in the other (there are only two listeners).
    ExtensionTestMessageListener success_listener(
        test.expected_success_message);
    ExtensionTestMessageListener skip_listener(test.expected_skip_message);
    DispatchOnResultEvent(test.key, test.transcript, test.is_final);
    ASSERT_TRUE(success_listener.WaitUntilSatisfied());
    ASSERT_TRUE(skip_listener.WaitUntilSatisfied());
  }
}

// Tests that events can be dispatched from the SpeechRecognitionPrivateManager
// and received and processed in an extension.
IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateManagerTest,
                       DispatchOnErrorEvent) {
  ResultCatcher result_catcher;
  ExtensionTestMessageListener listener("Proceed");

  const Extension* extension = LoadExtensionAsComponent("onerror_event");
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  DispatchOnErrorEvent(kExtensionId, "A fatal error");
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace extensions
