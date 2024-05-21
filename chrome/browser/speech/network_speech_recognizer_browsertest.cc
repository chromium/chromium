// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/network_speech_recognizer.h"

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "content/public/test/test_utils.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
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
           const std::optional<media::SpeechRecognitionResult>& timing));
  MOCK_METHOD1(OnSpeechSoundLevelChanged, void(int16_t));
  MOCK_METHOD1(OnSpeechRecognitionStateChanged, void(SpeechRecognizerStatus));
  MOCK_METHOD0(OnSpeechRecognitionStopped, void());
  MOCK_METHOD1(OnLanguageIdentificationEvent,
               void(media::mojom::LanguageIdentificationEventPtr));

 private:
  base::WeakPtrFactory<MockSpeechRecognizerDelegate> weak_factory_{this};
};

class NetworkSpeechRecognizerBrowserTest : public InProcessBrowserTest {
 public:
  NetworkSpeechRecognizerBrowserTest() = default;
  ~NetworkSpeechRecognizerBrowserTest() override = default;
  NetworkSpeechRecognizerBrowserTest(
      const NetworkSpeechRecognizerBrowserTest&) = delete;
  NetworkSpeechRecognizerBrowserTest& operator=(
      const NetworkSpeechRecognizerBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    fake_speech_recognition_manager_ =
        std::make_unique<content::FakeSpeechRecognitionManager>();
    fake_speech_recognition_manager_->set_should_send_fake_response(false);
    content::SpeechRecognitionManager::SetManagerForTesting(
        fake_speech_recognition_manager_.get());
    mock_speech_delegate_ = std::make_unique<MockSpeechRecognizerDelegate>();
  }

  void TearDownOnMainThread() override {
    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  }

 protected:
  std::unique_ptr<content::FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;
  std::unique_ptr<MockSpeechRecognizerDelegate> mock_speech_delegate_;
};

IN_PROC_BROWSER_TEST_F(NetworkSpeechRecognizerBrowserTest, RecognizeSpeech) {
  NetworkSpeechRecognizer recognizer(
      mock_speech_delegate_->GetWeakPtr(),
      browser()
          ->profile()
          ->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcessIOThread(),
      "en" /* accept_language */, "en" /* locale */);

  testing::InSequence seq;
  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_READY))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_RECOGNIZING))
      .Times(1);
  recognizer.Start();
  fake_speech_recognition_manager_->WaitForRecognitionStarted();
  base::RunLoop().RunUntilIdle();

  base::RunLoop first_response_loop;
  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_IN_SPEECH))
      .Times(1);
  EXPECT_CALL(
      *mock_speech_delegate_,
      OnSpeechResult(std::u16string(u"Pictures of the moon"), true, testing::_))
      .WillOnce(InvokeWithoutArgs(&first_response_loop, &base::RunLoop::Quit))
      .RetiresOnSaturation();
  fake_speech_recognition_manager_->SendFakeResponse(
      false /* end recognition */, base::DoNothing());
  first_response_loop.Run();

  // Try another speech response.
  fake_speech_recognition_manager_->SetFakeResult("Pictures of mars!",
                                                  /*is_final=*/true);
  base::RunLoop second_response_loop;
  EXPECT_CALL(
      *mock_speech_delegate_,
      OnSpeechResult(std::u16string(u"Pictures of mars!"), true, testing::_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_READY))
      .WillOnce(InvokeWithoutArgs(&second_response_loop, &base::RunLoop::Quit));
  fake_speech_recognition_manager_->SendFakeResponse(true /* end recognition */,
                                                     base::DoNothing());
  second_response_loop.Run();

  // Stop listening, no more callbacks expected.
  recognizer.Stop();
}
