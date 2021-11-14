// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_BASE_TEST_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_BASE_TEST_H_

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "chrome/browser/speech/speech_recognition_test_helper.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/features/feature_channel.h"

namespace extensions {

// This class automatically sets up and tears down the speech recognition
// service depending on a test's parameter value. It is used by various tests
// within the speech recognition private code base. Among these are the
// SpeechRecognitionPrivateApiTest (which needs the ExtensionApiTest
// infrastructure), as well as SpeechRecognitionPrivateManagerTest and
// SpeechRecognitionPrivateRecognizerTest. To support all of these tests with a
// single base class, this class inherits from ExtensionApiTest. Inheriting from
// any other test type, e.g. InProcessBrowserTest or BrowserTestBase, results in
// an inheritance diamond problem, since the SpeechRecognitionPrivateApiTest
// needs infrastructure from both this class and the ExtensionApiTest to run.
class SpeechRecognitionPrivateBaseTest
    : public ExtensionApiTest,
      public ::testing::WithParamInterface<speech::SpeechRecognitionType> {
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

  // Routers to SpeechRecognitionTestHelper methods.
  void WaitForRecognitionStarted();
  void WaitForRecognitionStopped();
  void SendFakeSpeechResultAndWait(const std::string& transcript,
                                   bool is_final);
  void SendFinalFakeSpeechResultAndWait(const std::string& transcript);
  void SendFakeSpeechRecognitionErrorAndWait();

 private:
  SpeechRecognitionTestHelper test_helper_;
  // Needed because the speechRecognitionPrivate API is restricted to the dev
  // channel during development.
  // TODO(crbug.com/1220107): Remove this when it's launched to stable.
  extensions::ScopedCurrentChannel current_channel_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_BASE_TEST_H_
