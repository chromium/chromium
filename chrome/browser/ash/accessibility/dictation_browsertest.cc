// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation.h"

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/mock_ime_input_context_handler.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method_base.h"

namespace ash {
namespace {

const char kFirstSpeechResult[] = "help";
const char kSecondSpeechResult[] = "help oh";
const char kFinalSpeechResult[] = "hello world";

}  // namespace

class DictationTest : public InProcessBrowserTest {
 protected:
  DictationTest() {
    input_context_handler_.reset(new ui::MockIMEInputContextHandler());
    empty_composition_text_ =
        ui::MockIMEInputContextHandler::UpdateCompositionTextArg()
            .composition_text;
  }
  ~DictationTest() override = default;
  DictationTest(const DictationTest&) = delete;
  DictationTest& operator=(const DictationTest&) = delete;

  void SetUp() override {
    // Use a fake speech recognition manager so that we don't end up with an
    // error finding the audio input device when running on a headless
    // environment.
    fake_speech_recognition_manager_.reset(
        new content::FakeSpeechRecognitionManager());
    // Don't send a fake response from the fake manager. The fake manager can
    // only send one final response before shutting off. We will do more
    // granular testing of multiple not-final and final results by sending
    // OnSpeechResult callbacks to Dictation directly.
    fake_speech_recognition_manager_->set_should_send_fake_response(false);
    content::SpeechRecognitionManager::SetManagerForTesting(
        fake_speech_recognition_manager_.get());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ui::IMEBridge::Get()->SetInputContextHandler(input_context_handler_.get());
  }

  AccessibilityManager* GetManager() { return AccessibilityManager::Get(); }

  void EnableChromeVox() { GetManager()->EnableSpokenFeedback(true); }

  void SendSpeechResult(const char* result, bool is_final) {
    GetManager()->dictation_->OnSpeechResult(base::ASCIIToUTF16(result),
                                             is_final, base::nullopt);
  }

  void NotifyTextInputStateChanged(ui::TextInputClient* client) {
    GetManager()->dictation_->OnTextInputStateChanged(client);
  }

  void ToggleDictation() {
    bool is_turning_on =
        !GetManager()->dictation_ ||
        GetManager()->dictation_->current_state_ == SPEECH_RECOGNIZER_OFF;
    GetManager()->ToggleDictation();
    if (is_turning_on) {
      // SpeechRecognitionManager is asynchronous: it is turned on from the UI
      // thread. Wait for it to complete before moving on to ensures that we are
      // ready to receive speech. In Dictation, a tone is played when
      // recognition starts, indicating to the user that they can begin
      // speaking.
      fake_speech_recognition_manager_->WaitForRecognitionStarted();
      // Now wait for the callbacks to propagate on the UI thread.
      base::RunLoop().RunUntilIdle();
    }
  }

  ui::CompositionText GetLastCompositionText() {
    return input_context_handler_->last_update_composition_arg()
        .composition_text;
  }

  std::unique_ptr<ui::MockIMEInputContextHandler> input_context_handler_;
  ui::CompositionText empty_composition_text_;
  std::unique_ptr<content::FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;
};

IN_PROC_BROWSER_TEST_F(DictationTest, RecognitionEnds) {
  ToggleDictation();
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kFirstSpeechResult, false /* is_final */);
  EXPECT_EQ(base::ASCIIToUTF16(kFirstSpeechResult),
            GetLastCompositionText().text);

  SendSpeechResult(kSecondSpeechResult, false /* is_final */);
  EXPECT_EQ(base::ASCIIToUTF16(kSecondSpeechResult),
            GetLastCompositionText().text);

  SendSpeechResult(kFinalSpeechResult, true /* is_final */);
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(base::ASCIIToUTF16(kFinalSpeechResult),
            input_context_handler_->last_commit_text());
}

IN_PROC_BROWSER_TEST_F(DictationTest, RecognitionEndsWithChromeVoxEnabled) {
  AccessibilityManager* manager = GetManager();
  EnableChromeVox();
  EXPECT_TRUE(manager->IsSpokenFeedbackEnabled());

  ToggleDictation();
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kFirstSpeechResult, false /* is_final */);
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kSecondSpeechResult, false /* is_final */);
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kFinalSpeechResult, true /* is_final */);
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(base::ASCIIToUTF16(kFinalSpeechResult),
            input_context_handler_->last_commit_text());
}

IN_PROC_BROWSER_TEST_F(DictationTest, UserEndsDictationBeforeSpeech) {
  ToggleDictation();
  ToggleDictation();
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);
  EXPECT_EQ(0, input_context_handler_->commit_text_call_count());
}

IN_PROC_BROWSER_TEST_F(DictationTest, UserEndsDictation) {
  ToggleDictation();
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kFinalSpeechResult, false /* is_final */);
  EXPECT_EQ(base::ASCIIToUTF16(kFinalSpeechResult),
            GetLastCompositionText().text);

  ToggleDictation();
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(base::ASCIIToUTF16(kFinalSpeechResult),
            input_context_handler_->last_commit_text());
}

IN_PROC_BROWSER_TEST_F(DictationTest, UserEndsDictationWhenChromeVoxEnabled) {
  AccessibilityManager* manager = GetManager();

  EnableChromeVox();
  EXPECT_TRUE(manager->IsSpokenFeedbackEnabled());

  ToggleDictation();
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kFinalSpeechResult, false /* is_final */);
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  ToggleDictation();
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(base::ASCIIToUTF16(kFinalSpeechResult),
            input_context_handler_->last_commit_text());
}

IN_PROC_BROWSER_TEST_F(DictationTest, SwitchInputContext) {
  // Turn on dictation and say something.
  ToggleDictation();
  SendSpeechResult(kFirstSpeechResult, true /* is_final */);

  // Speech goes to the default IMEInputContextHandler.
  EXPECT_EQ(base::ASCIIToUTF16(kFirstSpeechResult),
            input_context_handler_->last_commit_text());

  // Simulate a remote app instantiating a new IMEInputContextHandler, like
  // the keyboard shortcut viewer app creating a second InputMethodChromeOS.
  ui::MockIMEInputContextHandler input_context_handler2;
  ui::IMEBridge::Get()->SetInputContextHandler(&input_context_handler2);

  // Wait for speech to be stopped before continuing, or we might try to
  // turn it off with ToggleDictation below.
  fake_speech_recognition_manager_->WaitForRecognitionEnded();
  // Now wait for the callbacks to propagate on the UI thread.
  base::RunLoop().RunUntilIdle();

  // Turn on dictation and say something else.
  ToggleDictation();
  SendSpeechResult(kSecondSpeechResult, true /* is_final */);

  // Speech goes to the new IMEInputContextHandler.
  EXPECT_EQ(base::ASCIIToUTF16(kSecondSpeechResult),
            input_context_handler2.last_commit_text());

  ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
}

IN_PROC_BROWSER_TEST_F(DictationTest, ChangeInputField) {
  // Turn on dictation and start speaking.
  ToggleDictation();
  SendSpeechResult(kFinalSpeechResult, false /* is_final */);

  // Change the input state to a new client.
  std::unique_ptr<ui::TextInputClient> new_client =
      std::make_unique<ui::DummyTextInputClient>();
  NotifyTextInputStateChanged(new_client.get());

  // Check that dictation has turned off.
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(base::ASCIIToUTF16(kFinalSpeechResult),
            input_context_handler_->last_commit_text());
}

}  // namespace ash
