// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/network_speech_recognizer.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
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

  MOCK_METHOD3(OnSpeechResult,
               void(const base::string16& text,
                    bool is_final,
                    base::Optional<std::vector<base::TimeDelta>> word_offsets));
  MOCK_METHOD1(OnSpeechSoundLevelChanged, void(int16_t));
  MOCK_METHOD1(OnSpeechRecognitionStateChanged, void(SpeechRecognizerStatus));

 private:
  base::WeakPtrFactory<MockSpeechRecognizerDelegate> weak_factory_{this};
};

class AppListNetworkSpeechRecognizerBrowserTest : public InProcessBrowserTest {
 public:
  AppListNetworkSpeechRecognizerBrowserTest() {}

  AppListNetworkSpeechRecognizerBrowserTest(
      const AppListNetworkSpeechRecognizerBrowserTest&) = delete;
  AppListNetworkSpeechRecognizerBrowserTest& operator=(
      const AppListNetworkSpeechRecognizerBrowserTest&) = delete;

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
};

IN_PROC_BROWSER_TEST_F(AppListNetworkSpeechRecognizerBrowserTest,
                       RecognizeSpeech) {
  NetworkSpeechRecognizer recognizer(
      mock_speech_delegate_->GetWeakPtr(),
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
          ->GetURLLoaderFactoryForBrowserProcessIOThread(),
      "en" /* accept_language */, "en" /* locale */);

  base::RunLoop run_loop;
  base::Optional<std::vector<base::TimeDelta>> timings = base::nullopt;
  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechResult(base::ASCIIToUTF16("Pictures of the moon"), true,
                             timings));
  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_READY))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  recognizer.Start();
  run_loop.Run();
}
