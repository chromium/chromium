// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation.h"

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/accessibility/soda_installer.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/soda_installer_impl_chromeos.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
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

enum DictationListeningTestVariant {
  kTestDefaultListening,
  kTestWithLongerListening
};
enum DictationNetworkTestVariant { kNetworkRecognition, kOnDeviceRecognition };

class DictationTest : public InProcessBrowserTest,
                      public ::testing::WithParamInterface<
                          std::pair<DictationListeningTestVariant,
                                    DictationNetworkTestVariant>> {
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

  // InProcessBrowserTest:
  void SetUp() override {
    if (GetParam().second == kNetworkRecognition) {
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
    } else {
      // Fake that SODA is installed so Dictation uses OnDeviceSpeechRecognizer.
      static_cast<speech::SodaInstallerImplChromeOS*>(
          speech::SodaInstaller::GetInstance())
          ->soda_installed_for_test_ = true;
    }
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam().first == kTestWithLongerListening) {
      command_line->AppendSwitch(
          switches::kEnableExperimentalAccessibilityDictationListening);
    }
    if (GetParam().second == kOnDeviceRecognition) {
      command_line->AppendSwitch(
          switches::kEnableExperimentalAccessibilityDictationOffline);
    }
  }

  void SetUpOnMainThread() override {
    ui::IMEBridge::Get()->SetInputContextHandler(input_context_handler_.get());
    if (GetParam().second == kOnDeviceRecognition) {
      // Replaces normal CrosSpeechRecognitionService with a fake one.
      CrosSpeechRecognitionServiceFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              browser()->profile(),
              base::BindRepeating(
                  &DictationTest::CreateTestSpeechRecognitionService,
                  base::Unretained(this)));
    }
  }

  std::unique_ptr<KeyedService> CreateTestSpeechRecognitionService(
      content::BrowserContext* context) {
    std::unique_ptr<speech::FakeSpeechRecognitionService> fake_service =
        std::make_unique<speech::FakeSpeechRecognitionService>();
    fake_service_ = fake_service.get();
    return std::move(fake_service);
  }

  AccessibilityManager* GetManager() { return AccessibilityManager::Get(); }

  void EnableChromeVox() { GetManager()->EnableSpokenFeedback(true); }

  void SendSpeechResult(const char* result, bool is_final) {
    if (GetParam().second == kNetworkRecognition) {
      if (!is_final) {
        // FakeSpeechRecognitionManager can only send final results,
        // so if this isn't final just send to Dictation directly.
        GetManager()->dictation_->OnSpeechResult(base::ASCIIToUTF16(result),
                                                 is_final, base::nullopt);
      } else {
        base::RunLoop loop;
        fake_speech_recognition_manager_->SetFakeResult(result);
        fake_speech_recognition_manager_->SendFakeResponse(
            false /* end recognition */, loop.QuitClosure());
        loop.Run();
      }
    } else {
      EXPECT_TRUE(fake_service_->is_capturing_audio());
      base::RunLoop loop;
      fake_service_->SendSpeechRecognitionResult(
          media::mojom::SpeechRecognitionResult::New(result, is_final));
      loop.RunUntilIdle();
    }
  }

  void WaitForRecognitionStarted() {
    if (GetParam().second == kNetworkRecognition) {
      // Wait for interaction on UI thread.
      fake_speech_recognition_manager_->WaitForRecognitionStarted();
    } else {
      fake_service_->WaitForRecognitionStarted();
      // Only one thread, use a RunLoop to ensure mojom messages are done.
      base::RunLoop().RunUntilIdle();
    }
  }

  void WaitForRecognitionEnded() {
    if (GetParam().second == kNetworkRecognition) {
      // Wait for interaction on UI thread.
      fake_speech_recognition_manager_->WaitForRecognitionEnded();
    }
    // Wait for mojom / callbacks to propagate.
    base::RunLoop().RunUntilIdle();
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
      // SpeechRecognition may be turned on asynchronously. Wait for it to
      // complete before moving on to ensures that we are ready to receive
      // speech. In Dictation, a tone is played when recognition starts,
      // indicating to the user that they can begin speaking.
      WaitForRecognitionStarted();
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

  // For network recognition.
  std::unique_ptr<content::FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;

  // For on-device recognition.
  // Unowned.
  speech::FakeSpeechRecognitionService* fake_service_;
};

INSTANTIATE_TEST_SUITE_P(
    TestWithDefaultAndLongerListening,
    DictationTest,
    ::testing::Values(
        std::pair<DictationListeningTestVariant, DictationNetworkTestVariant>(
            kTestDefaultListening,
            kNetworkRecognition),
        std::pair<DictationListeningTestVariant, DictationNetworkTestVariant>(
            kTestWithLongerListening,
            kNetworkRecognition),
        std::pair<DictationListeningTestVariant, DictationNetworkTestVariant>(
            kTestDefaultListening,
            kOnDeviceRecognition),
        std::pair<DictationListeningTestVariant, DictationNetworkTestVariant>(
            kTestWithLongerListening,
            kOnDeviceRecognition)));

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
  SendSpeechResult(kFirstSpeechResult, true /* is final */);

  // Speech goes to the default IMEInputContextHandler.
  EXPECT_EQ(base::ASCIIToUTF16(kFirstSpeechResult),
            input_context_handler_->last_commit_text());

  // Simulate a remote app instantiating a new IMEInputContextHandler, like
  // the keyboard shortcut viewer app creating a second InputMethodChromeOS.
  ui::MockIMEInputContextHandler input_context_handler2;
  ui::IMEBridge::Get()->SetInputContextHandler(&input_context_handler2);

  if (GetParam().first == kTestDefaultListening) {
    // Wait for speech to stop, then turn it on again.
    WaitForRecognitionEnded();
    EXPECT_TRUE(IsDictationOff());

    // Turn on dictation and say something else.
    ToggleDictation();
  }

  SendSpeechResult(kSecondSpeechResult, true /* is final*/);

  std::u16string expected =
      GetParam().first == kTestDefaultListening
          ? base::ASCIIToUTF16(kSecondSpeechResult)
          : u" " + base::ASCIIToUTF16(kSecondSpeechResult);

  // Speech goes to the new IMEInputContextHandler.
  EXPECT_EQ(expected, input_context_handler2.last_commit_text());

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
  SendSpeechResult("Purple", true /* is final */);

  EXPECT_EQ(u"Purple", input_context_handler_->last_commit_text());
  if (GetParam().first == kTestDefaultListening) {
    // Dictation should turn off.
    WaitForRecognitionEnded();
    EXPECT_TRUE(IsDictationOff());
    return;
  }
  EXPECT_FALSE(IsDictationOff());

  SendSpeechResult("pink", true /* is final */);
  EXPECT_EQ(2, input_context_handler_->commit_text_call_count());
  // Space in front of the result.
  EXPECT_EQ(u" pink", input_context_handler_->last_commit_text());

  SendSpeechResult(" blue", true /* is final */);
  EXPECT_EQ(3, input_context_handler_->commit_text_call_count());
  // Only one space in front of the result.
  EXPECT_EQ(u" blue", input_context_handler_->last_commit_text());

  ToggleDictation();
  // No change expected after toggle.
  EXPECT_EQ(3, input_context_handler_->commit_text_call_count());
}

}  // namespace ash
