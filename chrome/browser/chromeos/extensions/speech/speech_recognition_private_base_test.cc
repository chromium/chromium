// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_base_test.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/browser/ui/browser.h"
#include "components/soda/soda_installer.h"
#include "components/version_info/channel.h"
#include "content/public/test/fake_speech_recognition_manager.h"

namespace extensions {

SpeechRecognitionPrivateBaseTest::SpeechRecognitionPrivateBaseTest()
    : current_channel_(version_info::Channel::DEV) {}
SpeechRecognitionPrivateBaseTest::~SpeechRecognitionPrivateBaseTest() = default;

void SpeechRecognitionPrivateBaseTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  if (GetParam() == kOnDeviceRecognition) {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kOnDeviceSpeechRecognition);
  } else if (GetParam() == kNetworkRecognition) {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kOnDeviceSpeechRecognition);
  }

  ExtensionApiTest::SetUpCommandLine(command_line);
}

void SpeechRecognitionPrivateBaseTest::SetUpOnMainThread() {
  if (GetParam() == kOnDeviceRecognition)
    SetUpOnDeviceRecognition();
  else if (GetParam() == kNetworkRecognition)
    SetUpNetworkRecognition();

  ExtensionApiTest::SetUpOnMainThread();
}

void SpeechRecognitionPrivateBaseTest::TearDownOnMainThread() {
  if (GetParam() == kNetworkRecognition)
    content::SpeechRecognitionManager::SetManagerForTesting(nullptr);

  ExtensionApiTest::TearDownOnMainThread();
}

void SpeechRecognitionPrivateBaseTest::SetUpNetworkRecognition() {
  fake_speech_recognition_manager_ =
      std::make_unique<content::FakeSpeechRecognitionManager>();
  fake_speech_recognition_manager_->set_should_send_fake_response(false);
  content::SpeechRecognitionManager::SetManagerForTesting(
      fake_speech_recognition_manager_.get());
}

void SpeechRecognitionPrivateBaseTest::SetUpOnDeviceRecognition() {
  // Fake that SODA is installed so SpeechRecognitionPrivate uses
  // OnDeviceSpeechRecognizer.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  CrosSpeechRecognitionServiceFactory::GetInstanceForTest()
      ->SetTestingFactoryAndUse(
          browser()->profile(),
          base::BindRepeating(&SpeechRecognitionPrivateBaseTest::
                                  CreateTestOnDeviceSpeechRecognitionService,
                              base::Unretained(this)));
}

std::unique_ptr<KeyedService>
SpeechRecognitionPrivateBaseTest::CreateTestOnDeviceSpeechRecognitionService(
    content::BrowserContext* context) {
  std::unique_ptr<speech::FakeSpeechRecognitionService> fake_service =
      std::make_unique<speech::FakeSpeechRecognitionService>();
  fake_service_ = fake_service.get();
  return std::move(fake_service);
}

void SpeechRecognitionPrivateBaseTest::WaitForRecognitionStarted() {
  if (GetParam() == kNetworkRecognition) {
    fake_speech_recognition_manager_->WaitForRecognitionStarted();
  } else if (GetParam() == kOnDeviceRecognition) {
    fake_service_->WaitForRecognitionStarted();
  }
  base::RunLoop().RunUntilIdle();
}

void SpeechRecognitionPrivateBaseTest::WaitForRecognitionStopped() {
  if (GetParam() == kNetworkRecognition)
    fake_speech_recognition_manager_->WaitForRecognitionEnded();
  base::RunLoop().RunUntilIdle();
}

void SpeechRecognitionPrivateBaseTest::SendFinalFakeSpeechResultAndWait(
    const std::string& transcript) {
  base::RunLoop loop;
  if (GetParam() == kNetworkRecognition) {
    fake_speech_recognition_manager_->SetFakeResult(transcript);
    fake_speech_recognition_manager_->SendFakeResponse(
        false /* end recognition */, loop.QuitClosure());
    loop.Run();
  } else if (GetParam() == kOnDeviceRecognition) {
    EXPECT_TRUE(fake_service_->is_capturing_audio());
    fake_service_->SendSpeechRecognitionResult(
        media::SpeechRecognitionResult(transcript, /*is_final=*/true));
    loop.RunUntilIdle();
  }
}

void SpeechRecognitionPrivateBaseTest::SendFakeSpeechRecognitionErrorAndWait() {
  base::RunLoop loop;
  if (GetParam() == kNetworkRecognition) {
    fake_speech_recognition_manager_->SendFakeError(loop.QuitClosure());
    loop.Run();
  } else if (GetParam() == kOnDeviceRecognition) {
    fake_service_->SendSpeechRecognitionError();
    loop.RunUntilIdle();
  }
}

}  // namespace extensions
