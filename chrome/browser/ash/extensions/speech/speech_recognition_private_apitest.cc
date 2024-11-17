// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/speech/speech_recognition_private_api.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_base_test.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_manager.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_manager_factory.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_recognizer.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

class SpeechRecognitionPrivateApiTest
    : public SpeechRecognitionPrivateBaseTest {
 protected:
  SpeechRecognitionPrivateApiTest() {}
  ~SpeechRecognitionPrivateApiTest() override = default;
  SpeechRecognitionPrivateApiTest(const SpeechRecognitionPrivateApiTest&) =
      delete;
  SpeechRecognitionPrivateApiTest& operator=(
      const SpeechRecognitionPrivateApiTest&) = delete;

  void TearDownOnMainThread() override {
    SpeechRecognitionPrivateManagerFactory::GetForBrowserContext(profile())
        ->recognition_data_.clear();
    SpeechRecognitionPrivateBaseTest::TearDownOnMainThread();
  }
};

INSTANTIATE_TEST_SUITE_P(
    Network,
    SpeechRecognitionPrivateApiTest,
    ::testing::Values(speech::SpeechRecognitionType::kNetwork));

INSTANTIATE_TEST_SUITE_P(
    OnDevice,
    SpeechRecognitionPrivateApiTest,
    ::testing::Values(speech::SpeechRecognitionType::kOnDevice));

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateApiTest, Simple) {
  ASSERT_TRUE(RunSpeechRecognitionPrivateTest("simple")) << message_;
}

// An end-to-end test that starts speech recognition and ensures that the
// callback receives the correct boolean parameter for specifying on-device or
// network speech.
IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateApiTest, OnStart) {
  SetCustomArg(api::speech_recognition_private::ToString(
      speech::SpeechRecognitionTypeToApiType(GetParam())));
  ASSERT_TRUE(RunSpeechRecognitionPrivateTest("on_start")) << message_;
}

// An end-to-end test that starts speech recognition, waits for results, then
// finally stops recognition.
IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateApiTest, StartResultStop) {
  // This test requires some back and forth communication between C++ and JS.
  // Use message listeners to force the synchronicity of this test.
  ExtensionTestMessageListener start_listener("Started");
  ExtensionTestMessageListener first_result_listener("Received first result");
  ExtensionTestMessageListener second_result_listener(
      "Received second result", ReplyBehavior::kWillReply);

  // Load the extension and wait for speech recognition to start.
  ResultCatcher result_catcher;
  const Extension* extension = LoadExtensionAsComponent("start_result_stop");
  ASSERT_TRUE(extension);
  ASSERT_TRUE(start_listener.WaitUntilSatisfied());

  SendInterimResultAndWait("First result");
  ASSERT_TRUE(first_result_listener.WaitUntilSatisfied());
  ASSERT_FALSE(second_result_listener.was_satisfied());

  SendFinalResultAndWait("Second result");
  ASSERT_TRUE(second_result_listener.WaitUntilSatisfied());

  // Replying will trigger the extension to stop speech recogntition. As done
  // above, wait for the extension to confirm that recognition has stopped.
  second_result_listener.Reply("Proceed");
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// An end-to-end test that starts speech recognition, fires a fake error, then
// waits for the extension to handle both an onError and an onStop event.
IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateApiTest, StartErrorStop) {
  ExtensionTestMessageListener start_listener("Started");

  ResultCatcher result_catcher;
  const Extension* extension = LoadExtensionAsComponent("start_error_stop");
  ASSERT_TRUE(extension);
  ASSERT_TRUE(start_listener.WaitUntilSatisfied());

  SendErrorAndWait();
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace extensions
