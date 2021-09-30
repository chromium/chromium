// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_BASE_TEST_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_BASE_TEST_H_

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/features/feature_channel.h"

namespace content {
class FakeSpeechRecognitionManager;
}  // namespace content

namespace speech {
class FakeSpeechRecognitionService;
}  // namespace speech

namespace extensions {

enum SpeechRecognitionPrivateTestVariant {
  kNetworkRecognition,
  kOnDeviceRecognition
};

// This class provides on-device and network speech recognition test
// infrastructure. It automatically sets up and tears down the speech
// recognition service depending on a test's parameter value. Test classes can
// extend this class to easily interact with speech recognizers. For example:
//
// SpeechRecognizer* recognizer->Start();
// SpeechRecognitionPrivateBaseTest::WaitForRecognitionStarted();
// ...Continue with test...
//
// Note: this class is used by various tests within the speech recognition
// private code base. Among these are the SpeechRecognitionPrivateApiTest (which
// needs the ExtensionApiTest infrastructure), as well as
// SpeechRecognitionPrivateManagerTest and
// SpeechRecognitionPrivateRecognizerTest. To support all of these tests with a
// single base class, this class inherits from ExtensionApiTest. Inheriting from
// any other test type, e.g. InProcessBrowserTest or BrowserTestBase, results in
// an inheritance diamond problem, since the SpeechRecognitionPrivateApiTest
// needs infrastructure from both this class and the ExtensionApiTest to run.
class SpeechRecognitionPrivateBaseTest
    : public ExtensionApiTest,
      public ::testing::WithParamInterface<
          SpeechRecognitionPrivateTestVariant> {
 protected:
  SpeechRecognitionPrivateBaseTest();
  ~SpeechRecognitionPrivateBaseTest() override;
  SpeechRecognitionPrivateBaseTest(const SpeechRecognitionPrivateBaseTest&) =
      delete;
  SpeechRecognitionPrivateBaseTest& operator=(
      const SpeechRecognitionPrivateBaseTest&) = delete;

  // ExtensionApiTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Waits for the speech recognition service to start.
  void WaitForRecognitionStarted();
  // Waits for the speech recognition service to stop.
  void WaitForRecognitionStopped();
  // Sends a fake final speech result and waits for tasks to finish.
  void SendFinalFakeSpeechResultAndWait(const std::string& transcript);
  // Sends a fake speech recognition error and waits for tasks to finish.
  void SendFakeSpeechRecognitionErrorAndWait();

 private:
  // Methods for additional setup.
  void SetUpNetworkRecognition();
  void SetUpOnDeviceRecognition();
  std::unique_ptr<KeyedService> CreateTestOnDeviceSpeechRecognitionService(
      content::BrowserContext* context);

  // For network recognition.
  std::unique_ptr<content::FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;
  // For on-device recognition. KeyedService owned by the test profile.
  speech::FakeSpeechRecognitionService* fake_service_;
  // Needed because the speechRecognitionPrivate API is restricted to the dev
  // channel during development.
  // TODO(crbug.com/1220107): Remove this when it's launched to stable.
  extensions::ScopedCurrentChannel current_channel_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_BASE_TEST_H_
