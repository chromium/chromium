// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_TEST_HELPER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_TEST_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
class FakeSpeechRecognitionManager;
}  // namespace content

namespace speech {
class FakeSpeechRecognitionService;
enum class SpeechRecognitionType;
}  // namespace speech

// This class provides on-device and network speech recognition test
// infrastructure. Test classes can use this one to easily interact with
// speech recognizers. For example:
//
// SpeechRecognitionTestHelper* test_helper_ = ...;
// SpeechRecognizer* recognizer->Start();
// test_helper_->WaitForRecognitionStarted();
// ... Continue with test ...
//
// For examples, please see SpeechRecognitionPrivateBaseTest or
// DictationBaseTest.
class SpeechRecognitionTestHelper {
 public:
  explicit SpeechRecognitionTestHelper(speech::SpeechRecognitionType type);
  ~SpeechRecognitionTestHelper();
  SpeechRecognitionTestHelper(const SpeechRecognitionTestHelper&) = delete;
  SpeechRecognitionTestHelper& operator=(const SpeechRecognitionTestHelper&) =
      delete;

  // Sets up either on-device or network speech recognition.
  void SetUp(Profile* profile);
  // Waits for the speech recognition service to start.
  void WaitForRecognitionStarted();
  // Waits for the speech recognition service to stop.
  void WaitForRecognitionStopped();
  // Sends an interim (non-finalized) fake speech result and waits for tasks to
  // finish.
  void SendInterimResultAndWait(const std::string& transcript);
  // Sends a final fake speech result and waits for tasks to finish.
  void SendFinalResultAndWait(const std::string& transcript);
  // Sends a fake speech recognition error and waits for tasks to finish.
  void SendErrorAndWait();
  // Returns a list of features that should be enabled.
  std::vector<base::test::FeatureRef> GetEnabledFeatures();
  // Returns a list of features that should be disabled.
  std::vector<base::test::FeatureRef> GetDisabledFeatures();

 private:
  // Methods for setup.
  void SetUpNetworkRecognition();
  void SetUpOnDeviceRecognition(Profile* profile);
  std::unique_ptr<KeyedService> CreateTestOnDeviceSpeechRecognitionService(
      content::BrowserContext* context);

  // Sends a fake speech result and waits for tasks to finish.
  void SendFakeSpeechResultAndWait(const std::string& transcript,
                                   bool is_final);

  speech::SpeechRecognitionType type_;
  // For network recognition.
  std::unique_ptr<content::FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;
  // For on-device recognition. KeyedService owned by the test profile.
  raw_ptr<speech::FakeSpeechRecognitionService, ExperimentalAsh> fake_service_;
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_TEST_HELPER_H_
