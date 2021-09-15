// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_api.h"

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_base_test.h"
#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_manager.h"
#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_recognizer.h"

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
    SpeechRecogntionPrivateManager::GetInstance()->recognition_data_.clear();
    SpeechRecognitionPrivateBaseTest::TearDownOnMainThread();
  }
};

INSTANTIATE_TEST_SUITE_P(Network,
                         SpeechRecognitionPrivateApiTest,
                         ::testing::Values(kNetworkRecognition));

INSTANTIATE_TEST_SUITE_P(OnDevice,
                         SpeechRecognitionPrivateApiTest,
                         ::testing::Values(kOnDeviceRecognition));

IN_PROC_BROWSER_TEST_P(SpeechRecognitionPrivateApiTest, Start) {
  ASSERT_TRUE(RunExtensionTest("speech/speech_recognition_private/start"))
      << message_;
}

}  // namespace extensions
