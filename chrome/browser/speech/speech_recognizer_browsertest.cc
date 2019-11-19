// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognizer.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;
using ::testing::Return;

class MockSpeechRecognizerDelegate : public SpeechRecognizerDelegate {
 public:
  MockSpeechRecognizerDelegate() {}

  base::WeakPtr<MockSpeechRecognizerDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  MOCK_METHOD2(OnSpeechResult, void(const base::string16&, bool));
  MOCK_METHOD1(OnSpeechSoundLevelChanged, void(int16_t));
  MOCK_METHOD1(OnSpeechRecognitionStateChanged, void(SpeechRecognizerStatus));
  MOCK_METHOD2(GetSpeechAuthParameters, void(std::string*, std::string*));

 private:
  base::WeakPtrFactory<MockSpeechRecognizerDelegate> weak_factory_{this};
};

class AppListSpeechRecognizerBrowserTest : public InProcessBrowserTest {
 public:
  AppListSpeechRecognizerBrowserTest() {}

  void SetUpOnMainThread() override {
    fake_speech_recognition_manager_.reset(
        new content::FakeSpeechRecognitionManager());
    fake_speech_recognition_manager_->set_should_send_fake_response(true);
    content::SpeechRecognitionManager::SetManagerForTesting(
        fake_speech_recognition_manager_.get());
    mock_speech_delegate_.reset(new MockSpeechRecognizerDelegate());
  }

  void TearDownOnMainThread() override {
    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  }

 protected:
  std::unique_ptr<content::FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;
  std::unique_ptr<MockSpeechRecognizerDelegate> mock_speech_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListSpeechRecognizerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AppListSpeechRecognizerBrowserTest, RecognizeSpeech) {
  SpeechRecognizer recognizer(
      mock_speech_delegate_->GetWeakPtr(),
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
          ->GetURLLoaderFactoryForBrowserProcessIOThread(),
      "en" /* accept_language */, "en" /* locale */);

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechResult(base::ASCIIToUTF16("Pictures of the moon"), true));
  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_READY))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  recognizer.Start(nullptr);
  run_loop.Run();
}
