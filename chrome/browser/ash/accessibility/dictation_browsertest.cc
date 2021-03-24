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
#include "ui/accessibility/accessibility_switches.h"
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

enum DictationListeningTestVariant { kTestDefault, kTestWithLongerListening };

class DictationTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<DictationListeningTestVariant> {
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

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam() == kTestWithLongerListening) {
      command_line->AppendSwitch(
          switches::kEnableExperimentalAccessibilityDictationListening);
    }
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

  bool IsDictationOff() {
    return !GetManager()->dictation_ ||
           GetManager()->dictation_->current_state_ == SPEECH_RECOGNIZER_OFF;
  }

  void ToggleDictation() {
    // We are trying to toggle on if Dictation is currently off.
    bool will_toggle_on = IsDictationOff();
    GetManager()->ToggleDictation();
    if (will_toggle_on) {
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

INSTANTIATE_TEST_SUITE_P(TestWithDefaultAndLongerListening,
                         DictationTest,
                         ::testing::Values(kTestDefault,
                                           kTestWithLongerListening));

IN_PROC_BROWSER_TEST_P(DictationTest, RecognitionEnds) {
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

IN_PROC_BROWSER_TEST_P(DictationTest, RecognitionEndsWithChromeVoxEnabled) {
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

IN_PROC_BROWSER_TEST_P(DictationTest, UserEndsDictationBeforeSpeech) {
  ToggleDictation();
  ToggleDictation();
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);
  EXPECT_EQ(0, input_context_handler_->commit_text_call_count());
}

IN_PROC_BROWSER_TEST_P(DictationTest, UserEndsDictation) {
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

IN_PROC_BROWSER_TEST_P(DictationTest, UserEndsDictationWhenChromeVoxEnabled) {
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

IN_PROC_BROWSER_TEST_P(DictationTest, SwitchInputContext) {
  // Turn on dictation and say something.
  ToggleDictation();
  base::RunLoop loop1;
  fake_speech_recognition_manager_->SetFakeResult(kFirstSpeechResult);
  fake_speech_recognition_manager_->SendFakeResponse(false,
                                                     loop1.QuitClosure());
  loop1.Run();

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
  EXPECT_TRUE(IsDictationOff());

  // Turn on dictation and say something else.
  ToggleDictation();
  base::RunLoop loop2;
  fake_speech_recognition_manager_->SetFakeResult(kSecondSpeechResult);
  fake_speech_recognition_manager_->SendFakeResponse(false,
                                                     loop2.QuitClosure());
  loop2.Run();

  // Speech goes to the new IMEInputContextHandler.
  EXPECT_EQ(base::ASCIIToUTF16(kSecondSpeechResult),
            input_context_handler2.last_commit_text());

  ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
}

IN_PROC_BROWSER_TEST_P(DictationTest, ChangeInputField) {
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

IN_PROC_BROWSER_TEST_P(DictationTest, MightListenForMultipleResults) {
  // Turn on dictation and send a final result.
  ToggleDictation();
  base::RunLoop loop1;
  fake_speech_recognition_manager_->SetFakeResult("Purple");
  fake_speech_recognition_manager_->SendFakeResponse(
      false /* end recognition */, loop1.QuitClosure());
  loop1.Run();

  EXPECT_EQ(u"Purple", input_context_handler_->last_commit_text());
  if (GetParam() == kTestDefault) {
    // Dictation should turn off.
    EXPECT_TRUE(IsDictationOff());
    return;
  }
  EXPECT_FALSE(IsDictationOff());

  // Send another result.
  base::RunLoop loop2;
  fake_speech_recognition_manager_->SetFakeResult("pink");
  fake_speech_recognition_manager_->SendFakeResponse(
      false /* end recognition */, loop2.QuitClosure());
  loop2.Run();
  EXPECT_EQ(2, input_context_handler_->commit_text_call_count());
  // Space in front of the result.
  EXPECT_EQ(u" pink", input_context_handler_->last_commit_text());

  base::RunLoop loop3;
  fake_speech_recognition_manager_->SetFakeResult(" blue");
  fake_speech_recognition_manager_->SendFakeResponse(
      false /* end recognition */, loop3.QuitClosure());
  loop3.Run();
  EXPECT_EQ(3, input_context_handler_->commit_text_call_count());
  // Only one space in front of the result.
  EXPECT_EQ(u" blue", input_context_handler_->last_commit_text());

  ToggleDictation();
  // No change expected after toggle.
  EXPECT_EQ(3, input_context_handler_->commit_text_call_count());
}

}  // namespace ash
