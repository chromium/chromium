// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_api.h"

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_base_test.h"
#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_manager.h"
#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_recognizer.h"
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
    SpeechRecognitionPrivateManager::Get(profile())->recognition_data_.clear();
    SpeechRecognitionPrivateBaseTest::TearDownOnMainThread();
  }
};

INSTANTIATE_TEST_SUITE_P(Network,
                         SpeechRecognitionPrivateApiTest,
                         ::testing::Values(kNetworkRecognition));

INSTANTIATE_TEST_SUITE_P(OnDevice,
                         SpeechRecognitionPrivateApiTest,
                         ::testing::Values(kOnDeviceRecognition));

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateApiTest, Simple) {
  ASSERT_TRUE(RunExtensionTest("speech/speech_recognition_private/simple"))
      << message_;
}

// An end-to-end test that starts speech recognition, waits for results, then
// finally stops recognition.
IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateApiTest, StartResultStop) {
  // This test requires some back and forth communication between C++ and JS.
  // Use message listeners to force the synchronicity of this test.
  ExtensionTestMessageListener start_listener("Started", false);
  ExtensionTestMessageListener result_listener("Received result", true);

  // Load the extension and wait for speech recognition to start.
  ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "speech/speech_recognition_private/start_result_stop"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(start_listener.WaitUntilSatisfied());

  // Send a fake speech result and wait for confirmation from the extension.
  SendFinalFakeSpeechResultAndWait("Testing");
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());

  // Replying will trigger the extension to stop speech recogntition. As done
  // above, wait for the extension to confirm that recognition has stopped.
  result_listener.Reply("Proceed");
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// An end-to-end test that starts speech recognition, fires a fake error, then
// waits for the extension to handle both an onError and an onStop event.
IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateApiTest, StartErrorStop) {
  ExtensionTestMessageListener start_listener("Started", false);

  ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "speech/speech_recognition_private/start_error_stop"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(start_listener.WaitUntilSatisfied());

  SendFakeSpeechRecognitionErrorAndWait();
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace extensions
