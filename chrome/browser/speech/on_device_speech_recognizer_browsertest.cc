// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_speech_recognizer.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::DoDefault;
using ::testing::InvokeWithoutArgs;

class MockSpeechRecognizerDelegate : public SpeechRecognizerDelegate {
 public:
  MockSpeechRecognizerDelegate() {}

  base::WeakPtr<MockSpeechRecognizerDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  MOCK_METHOD3(
      OnSpeechResult,
      void(const std::u16string& text,
           bool is_final,
           const base::Optional<SpeechRecognizerDelegate::TranscriptTiming>&
               timing));
  MOCK_METHOD1(OnSpeechSoundLevelChanged, void(int16_t));
  MOCK_METHOD1(OnSpeechRecognitionStateChanged, void(SpeechRecognizerStatus));

 private:
  base::WeakPtrFactory<MockSpeechRecognizerDelegate> weak_factory_{this};
};

// Tests OnDeviceSpeechRecognizer plumbing with a fake SpeechRecognitionService.
// Does not do end-to-end audio fetching or test SODA on device.
class OnDeviceSpeechRecognizerBrowsertest : public InProcessBrowserTest {
 public:
  OnDeviceSpeechRecognizerBrowsertest() = default;
  ~OnDeviceSpeechRecognizerBrowsertest() override = default;
  OnDeviceSpeechRecognizerBrowsertest(
      const OnDeviceSpeechRecognizerBrowsertest&) = delete;
  OnDeviceSpeechRecognizerBrowsertest& operator=(
      const OnDeviceSpeechRecognizerBrowsertest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Replaces normal CrosSpeechRecognitionService with a fake one.
    CrosSpeechRecognitionServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        browser()->profile(),
        base::BindRepeating(&OnDeviceSpeechRecognizerBrowsertest::
                                CreateTestSpeechRecognitionService,
                            base::Unretained(this)));
    mock_speech_delegate_.reset(
        new testing::StrictMock<MockSpeechRecognizerDelegate>());
  }

  std::unique_ptr<KeyedService> CreateTestSpeechRecognitionService(
      content::BrowserContext* context) {
    std::unique_ptr<speech::FakeSpeechRecognitionService> fake_service =
        std::make_unique<speech::FakeSpeechRecognitionService>();
    fake_service_ = fake_service.get();
    return std::move(fake_service);
  }

  void ConstructRecognizerAndWaitForReady() {
    base::RunLoop loop;
    EXPECT_CALL(*mock_speech_delegate_,
                OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_READY))
        .WillOnce(InvokeWithoutArgs(&loop, &base::RunLoop::Quit))
        .RetiresOnSaturation();
    recognizer_ = std::make_unique<OnDeviceSpeechRecognizer>(
        mock_speech_delegate_->GetWeakPtr(), browser()->profile());
    loop.Run();
  }

  std::unique_ptr<testing::StrictMock<MockSpeechRecognizerDelegate>>
      mock_speech_delegate_;
  std::unique_ptr<OnDeviceSpeechRecognizer> recognizer_;

  // Unowned.
  speech::FakeSpeechRecognitionService* fake_service_;
};

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognizerBrowsertest,
                       SetsUpServiceConnection) {
  ConstructRecognizerAndWaitForReady();
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognizerBrowsertest,
                       StartsCapturingAudio) {
  testing::InSequence seq;
  ConstructRecognizerAndWaitForReady();
  EXPECT_FALSE(fake_service_->is_capturing_audio());

  // Toggle a few times.
  for (int i = 0; i < 2; i++) {
    EXPECT_CALL(*mock_speech_delegate_,
                OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_RECOGNIZING))
        .Times(1)
        .RetiresOnSaturation();
    recognizer_->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(fake_service_->is_capturing_audio());

    EXPECT_CALL(*mock_speech_delegate_,
                OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_READY))
        .Times(1)
        .RetiresOnSaturation();
    recognizer_->Stop();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(fake_service_->is_capturing_audio());
  }
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognizerBrowsertest,
                       ReceivesRecognitionEvents) {
  testing::InSequence seq;
  ConstructRecognizerAndWaitForReady();

  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_RECOGNIZING))
      .Times(1)
      .RetiresOnSaturation();
  recognizer_->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_IN_SPEECH))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechResult(base::ASCIIToUTF16("All mammals have hair"), false,
                             testing::_))
      .Times(1)
      .RetiresOnSaturation();
  fake_service_->SendSpeechRecognitionResult(
      media::mojom::SpeechRecognitionResult::New("All mammals have hair",
                                                 false));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechResult(base::ASCIIToUTF16(
                                 "All mammals drink milk from their mothers"),
                             true, testing::_))
      .Times(1)
      .RetiresOnSaturation();
  fake_service_->SendSpeechRecognitionResult(
      media::mojom::SpeechRecognitionResult::New(
          "All mammals drink milk from their mothers", true));
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognizerBrowsertest, ReceivesErrors) {
  testing::InSequence seq;
  ConstructRecognizerAndWaitForReady();

  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_ERROR))
      .Times(1)
      .RetiresOnSaturation();
  fake_service_->SendSpeechRecognitionError();
  base::RunLoop().RunUntilIdle();
}
