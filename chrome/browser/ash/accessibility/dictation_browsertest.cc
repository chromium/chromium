// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_installer_impl_chromeos.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/mock_ime_input_context_handler.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

const char kFirstSpeechResult[] = "help";
const char16_t kFirstSpeechResult16[] = u"help";
const char kSecondSpeechResult[] = "help oh";
const char16_t kSecondSpeechResult16[] = u"help oh";
const char kFinalSpeechResult[] = "hello world";
const char16_t kFinalSpeechResult16[] = u"hello world";
const int kNoSpeechTimeoutInSeconds = 10;
const int kShortNoSpeechTimeoutInSeconds = 5;
const int kVeryShortNoSpeechTimeoutInSeconds = 2;

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
    input_context_handler_ = std::make_unique<ui::MockIMEInputContextHandler>();
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
      fake_speech_recognition_manager_ =
          std::make_unique<content::FakeSpeechRecognitionManager>();
      // Don't send a fake response from the fake manager. The fake manager can
      // only send one final response before shutting off. We will do more
      // granular testing of multiple not-final and final results by sending
      // OnSpeechResult callbacks to Dictation directly.
      fake_speech_recognition_manager_->set_should_send_fake_response(false);
      content::SpeechRecognitionManager::SetManagerForTesting(
          fake_speech_recognition_manager_.get());
    }

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;
    if (GetParam().first == kTestWithLongerListening) {
      enabled_features.push_back(
          features::kExperimentalAccessibilityDictationListening);
    } else {
      disabled_features.push_back(
          features::kExperimentalAccessibilityDictationListening);
    }
    if (GetParam().second == kOnDeviceRecognition) {
      enabled_features.push_back(
          features::kExperimentalAccessibilityDictationOffline);
    } else {
      disabled_features.push_back(
          features::kExperimentalAccessibilityDictationOffline);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUpOnMainThread() override {
    ui::IMEBridge::Get()->SetInputContextHandler(input_context_handler_.get());
    generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::Get()->GetPrimaryRootWindow());
    ash::Shell::Get()
        ->accessibility_controller()
        ->dictation()
        .SetDialogAccepted();
    ash::Shell::Get()->accessibility_controller()->dictation().SetEnabled(true);
    if (GetParam().second == kOnDeviceRecognition) {
      // Replaces normal CrosSpeechRecognitionService with a fake one.
      CrosSpeechRecognitionServiceFactory::GetInstanceForTest()
          ->SetTestingFactoryAndUse(
              browser()->profile(),
              base::BindRepeating(
                  &DictationTest::CreateTestSpeechRecognitionService,
                  base::Unretained(this)));

      // Fake that SODA is installed so Dictation uses OnDeviceSpeechRecognizer.
      // Do this here, since SetUpOnMainThread is run after the browser process
      // initializes (which is when the global SodaInstaller gets created).
      static_cast<speech::SodaInstallerImplChromeOS*>(
          speech::SodaInstaller::GetInstance())
          ->set_soda_installed_for_test(true);
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
                                                 is_final, absl::nullopt);
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
          media::SpeechRecognitionResult(result, is_final));
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

  base::OneShotTimer* GetTimer() {
    if (!GetManager()->dictation_)
      return nullptr;
    return &(GetManager()->dictation_->speech_timeout_);
  }

  base::TimeDelta GetNoSpeechTimeout() {
    if (GetParam().first == kTestDefaultListening) {
      return base::TimeDelta::FromSeconds(GetParam().second ==
                                                  kNetworkRecognition
                                              ? kShortNoSpeechTimeoutInSeconds
                                              : kNoSpeechTimeoutInSeconds);
    }
    return base::TimeDelta::FromSeconds(kNoSpeechTimeoutInSeconds);
  }

  base::TimeDelta GetNoNewSpeechTimeout() {
    return base::TimeDelta::FromSeconds(GetParam().second == kNetworkRecognition
                                            ? kVeryShortNoSpeechTimeoutInSeconds
                                            : kShortNoSpeechTimeoutInSeconds);
  }

  void ToggleDictation() {
    // We are trying to toggle on if Dictation is currently off.
    bool will_toggle_on = IsDictationOff();
    generator_->PressKey(ui::VKEY_D, ui::EF_COMMAND_DOWN);
    generator_->ReleaseKey(ui::VKEY_D, ui::EF_COMMAND_DOWN);
    if (will_toggle_on) {
      // SpeechRecognition may be turned on asynchronously. Wait for it to
      // complete before moving on to ensures that we are ready to receive
      // speech. In Dictation, a tone is played when recognition starts,
      // indicating to the user that they can begin speaking.
      WaitForRecognitionStarted();
    }
    // Now wait for the callbacks to propagate on the UI thread.
    base::RunLoop().RunUntilIdle();
  }

  ui::CompositionText GetLastCompositionText() {
    return input_context_handler_->last_update_composition_arg()
        .composition_text;
  }

  std::unique_ptr<ui::MockIMEInputContextHandler> input_context_handler_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  ui::CompositionText empty_composition_text_;

  // For network recognition.
  std::unique_ptr<content::FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;

  // For on-device recognition.
  // Unowned.
  speech::FakeSpeechRecognitionService* fake_service_;

  base::test::ScopedFeatureList scoped_feature_list_;
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
  EXPECT_EQ(kFirstSpeechResult16, GetLastCompositionText().text);

  SendSpeechResult(kSecondSpeechResult, false /* is_final */);
  EXPECT_EQ(kSecondSpeechResult16, GetLastCompositionText().text);

  SendSpeechResult(kFinalSpeechResult, true /* is_final */);
  // Wait for interim results to be finalized.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(kFinalSpeechResult16, input_context_handler_->last_commit_text());

  if (GetParam().first == kTestDefaultListening) {
    EXPECT_TRUE(IsDictationOff());
  } else {
    EXPECT_FALSE(IsDictationOff());
    base::OneShotTimer* timer = GetTimer();
    ASSERT_TRUE(timer);
    EXPECT_EQ(timer->GetCurrentDelay(), GetNoSpeechTimeout());
  }
}

IN_PROC_BROWSER_TEST_P(DictationTest, RecognitionEndsWithChromeVoxEnabled) {
  AccessibilityManager* manager = GetManager();
  EnableChromeVox();
  EXPECT_TRUE(manager->IsSpokenFeedbackEnabled());

  // Toggle Dictation on directly.
  GetManager()->ToggleDictation();
  WaitForRecognitionStarted();
  // Now wait for the callbacks to propagate on the UI thread.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kFirstSpeechResult, false /* is_final */);
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kSecondSpeechResult, false /* is_final */);
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kFinalSpeechResult, true /* is_final */);
  // Wait for interim results to be finalized.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(kFinalSpeechResult16, input_context_handler_->last_commit_text());

  if (GetParam().first == kTestDefaultListening) {
    EXPECT_TRUE(IsDictationOff());
  } else {
    EXPECT_FALSE(IsDictationOff());
    base::OneShotTimer* timer = GetTimer();
    ASSERT_TRUE(timer);
    EXPECT_EQ(timer->GetCurrentDelay(), GetNoSpeechTimeout());
  }
}

IN_PROC_BROWSER_TEST_P(DictationTest, RecognitionEndsWithNoSpeech) {
  ToggleDictation();
  EXPECT_FALSE(IsDictationOff());
  base::OneShotTimer* timer = GetTimer();
  ASSERT_TRUE(timer);
  EXPECT_EQ(timer->GetCurrentDelay(), GetNoSpeechTimeout());
  // Firing the timer, which simluates waiting for some time with no events,
  // should end dictation.
  timer->FireNow();
  EXPECT_TRUE(IsDictationOff());
}

IN_PROC_BROWSER_TEST_P(DictationTest, RecognitionEndsWithoutFinalizedSpeech) {
  ToggleDictation();
  EXPECT_FALSE(IsDictationOff());
  SendSpeechResult(kFirstSpeechResult, false /* is_final */);
  base::OneShotTimer* timer = GetTimer();
  ASSERT_TRUE(timer);
  // If this is the test with continuous listening, use the normal timeout,
  // otherwise use the shorter timeout for no new speech.
  EXPECT_EQ(timer->GetCurrentDelay(), GetParam().first == kTestDefaultListening
                                          ? GetNoNewSpeechTimeout()
                                          : GetNoSpeechTimeout());
  // Firing the timer, which simluates waiting for some time without new speech,
  // should end dictation.
  timer->FireNow();
  // Wait for interim results to be finalized.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsDictationOff());
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(kFirstSpeechResult16, input_context_handler_->last_commit_text());
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
  EXPECT_EQ(kFinalSpeechResult16, GetLastCompositionText().text);

  ToggleDictation();
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(kFinalSpeechResult16, input_context_handler_->last_commit_text());
}

IN_PROC_BROWSER_TEST_P(DictationTest, UserEndsDictationWhenChromeVoxEnabled) {
  AccessibilityManager* manager = GetManager();

  EnableChromeVox();
  EXPECT_TRUE(manager->IsSpokenFeedbackEnabled());

  // Toggle Dictation on directly.
  GetManager()->ToggleDictation();
  WaitForRecognitionStarted();
  // Now wait for the callbacks to propagate on the UI thread.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kFinalSpeechResult, false /* is_final */);
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  // Toggle Dictation off.
  GetManager()->ToggleDictation();
  base::RunLoop().RunUntilIdle();

  // Wait for interim results to be finalized.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(kFinalSpeechResult16, input_context_handler_->last_commit_text());
}

IN_PROC_BROWSER_TEST_P(DictationTest, SwitchInputContext) {
  // Turn on dictation and say something.
  ToggleDictation();
  SendSpeechResult(kFirstSpeechResult, true /* is final */);
  // Wait for interim results to be finalized.
  base::RunLoop().RunUntilIdle();

  // Speech goes to the default IMEInputContextHandler.
  EXPECT_EQ(kFirstSpeechResult16, input_context_handler_->last_commit_text());

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
  // Wait for interim results to be finalized.
  base::RunLoop().RunUntilIdle();

  std::u16string expected = kSecondSpeechResult16;
  if (GetParam().first != kTestDefaultListening)
    expected = u" " + expected;

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
  // Wait for interim results to be finalized.
  base::RunLoop().RunUntilIdle();

  // Check that dictation has turned off.
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(kFinalSpeechResult16, input_context_handler_->last_commit_text());
}

IN_PROC_BROWSER_TEST_P(DictationTest, MightListenForMultipleResults) {
  // Turn on dictation and send a final result.
  ToggleDictation();
  SendSpeechResult("Purple", true /* is final */);
  // Wait for interim results to be finalized.
  base::RunLoop().RunUntilIdle();

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
