// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/input_method/textinput_test_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_installer_impl_chromeos.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/mock_ime_input_context_handler.h"
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

static const char* kEnglishDictationCommands[] = {
    "delete",     "move left",    "move right", "move up", "move down",
    "copy",       "paste",        "cut",        "undo",    "redo",
    "select all", "unselect all", "new line"};

PrefService* GetActiveUserPrefs() {
  return ProfileManager::GetActiveUserProfile()->GetPrefs();
}

}  // namespace

enum DictationNetworkTestVariant { kNetworkRecognition, kOnDeviceRecognition };

class DictationTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<DictationNetworkTestVariant> {
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
    if (GetParam() == kNetworkRecognition) {
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
    if (GetParam() == kOnDeviceRecognition) {
      enabled_features.push_back(
          ::features::kExperimentalAccessibilityDictationOffline);
      enabled_features.push_back(ash::features::kOnDeviceSpeechRecognition);
    } else {
      disabled_features.push_back(
          ::features::kExperimentalAccessibilityDictationOffline);
      disabled_features.push_back(ash::features::kOnDeviceSpeechRecognition);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUpOnMainThread() override {
    // Fake that SODA is installed so Dictation uses OnDeviceSpeechRecognizer.
    // Do this here, since SetUpOnMainThread is run after the browser process
    // initializes (which is when the global SodaInstaller gets created).
    // Lastly, do this before Dictation is enabled so that we don't initiate a
    // SODA download when Dictation is enabled.
    if (GetParam() == kOnDeviceRecognition) {
      speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
    }

    ui::IMEBridge::Get()->SetInputContextHandler(input_context_handler_.get());
    generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::Get()->GetPrimaryRootWindow());
    GetActiveUserPrefs()->SetBoolean(
        prefs::kDictationAcceleratorDialogHasBeenAccepted, true);
    ash::Shell::Get()->accessibility_controller()->dictation().SetEnabled(true);
    if (GetParam() == kOnDeviceRecognition) {
      // Replaces normal CrosSpeechRecognitionService with a fake one.
      CrosSpeechRecognitionServiceFactory::GetInstanceForTest()
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

  void SendSpeechResult(const std::string& result, bool is_final) {
    if (GetParam() == kNetworkRecognition) {
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
    if (GetParam() == kNetworkRecognition) {
      // Wait for interaction on UI thread.
      fake_speech_recognition_manager_->WaitForRecognitionStarted();
    } else {
      fake_service_->WaitForRecognitionStarted();
      // Only one thread, use a RunLoop to ensure mojom messages are done.
      base::RunLoop().RunUntilIdle();
    }
  }

  void WaitForRecognitionEnded() {
    if (GetParam() == kNetworkRecognition) {
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

  void ToggleDictation() {
    // We are trying to toggle on if Dictation is currently off.
    bool will_toggle_on = IsDictationOff();
    generator_->PressAndReleaseKey(ui::VKEY_D, ui::EF_COMMAND_DOWN);
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

  const base::flat_map<std::string, Dictation::LocaleData>
  GetAllSupportedLocales() {
    return GetManager()->dictation_->GetAllSupportedLocales();
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

INSTANTIATE_TEST_SUITE_P(TestWithNetworkAndDeviceRecognition,
                         DictationTest,
                         ::testing::Values(kNetworkRecognition,
                                           kOnDeviceRecognition));

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

  EXPECT_FALSE(IsDictationOff());
  base::OneShotTimer* timer = GetTimer();
  ASSERT_TRUE(timer);
  EXPECT_EQ(timer->GetCurrentDelay(), base::Seconds(kNoSpeechTimeoutInSeconds));
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

  EXPECT_FALSE(IsDictationOff());
  base::OneShotTimer* timer = GetTimer();
  ASSERT_TRUE(timer);
  EXPECT_EQ(timer->GetCurrentDelay(), base::Seconds(kNoSpeechTimeoutInSeconds));
}

IN_PROC_BROWSER_TEST_P(DictationTest, RecognitionEndsWithNoSpeech) {
  ToggleDictation();
  EXPECT_FALSE(IsDictationOff());
  base::OneShotTimer* timer = GetTimer();
  ASSERT_TRUE(timer);
  EXPECT_EQ(timer->GetCurrentDelay(), base::Seconds(kNoSpeechTimeoutInSeconds));
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
  EXPECT_EQ(timer->GetCurrentDelay(), base::Seconds(kNoSpeechTimeoutInSeconds));
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
  // the keyboard shortcut viewer app creating a second `InputMethodAsh`.
  ui::MockIMEInputContextHandler input_context_handler2;
  ui::IMEBridge::Get()->SetInputContextHandler(&input_context_handler2);

  SendSpeechResult(kSecondSpeechResult, true /* is final*/);
  // Wait for interim results to be finalized.
  base::RunLoop().RunUntilIdle();

  std::u16string expected = u" ";
  expected += kSecondSpeechResult16;

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

IN_PROC_BROWSER_TEST_P(DictationTest, ListensForMultipleResults) {
  // Turn on dictation and send a final result.
  ToggleDictation();
  SendSpeechResult("Purple", true /* is final */);
  // Wait for interim results to be finalized.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(u"Purple", input_context_handler_->last_commit_text());
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

// Tests the behavior of the GetAllSupportedLocales method, specifically how
// it sets locale data.
IN_PROC_BROWSER_TEST_P(DictationTest, GetAllSupportedLocales) {
  if (GetParam() == kOnDeviceRecognition) {
    // Ensure that SODA is installed.
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  }

  auto locales = GetAllSupportedLocales();
  for (auto& it : locales) {
    const std::string locale = it.first;
    bool works_offline = it.second.works_offline;
    bool installed = it.second.installed;
    if (GetParam() == kOnDeviceRecognition &&
        locale == speech::kUsEnglishLocale) {
      // Currently, the only locale supported by SODA is en-US. It should work
      // offline and be installed.
      EXPECT_TRUE(works_offline);
      EXPECT_TRUE(installed);
    } else {
      EXPECT_FALSE(works_offline);
      EXPECT_FALSE(installed);
    }
  }

  if (GetParam() == kOnDeviceRecognition) {
    // Uninstall SODA and all language packs.
    speech::SodaInstaller::GetInstance()->UninstallSodaForTesting();
  } else {
    return;
  }

  locales = GetAllSupportedLocales();
  for (auto& it : locales) {
    const std::string locale = it.first;
    bool works_offline = it.second.works_offline;
    bool installed = it.second.installed;
    if (locale == speech::kUsEnglishLocale) {
      // en-US should be marked as "works offline", but it shouldn't be
      // installed.
      EXPECT_TRUE(works_offline);
      EXPECT_FALSE(installed);
    } else {
      EXPECT_FALSE(works_offline);
      EXPECT_FALSE(installed);
    }
  }
}

class TextMatchesWaiter {
 public:
  TextMatchesWaiter(const std::string& expected,
                    base::RepeatingCallback<std::string()> checker)
      : expected_(expected), checker_(std::move(checker)) {}
  ~TextMatchesWaiter() = default;
  TextMatchesWaiter(const TextMatchesWaiter&) = delete;
  TextMatchesWaiter& operator=(const TextMatchesWaiter&) = delete;

  void Wait() {
    base::RepeatingTimer check_timer;
    check_timer.Start(FROM_HERE, base::Milliseconds(10), this,
                      &TextMatchesWaiter::OnTimer);
    run_loop_.Run();
  }

 private:
  void OnTimer() {
    if (checker_.Run() == expected_)
      run_loop_.Quit();
  }

  std::string expected_;
  base::RepeatingCallback<std::string()> checker_;
  base::RunLoop run_loop_;
};

class DictationExtensionTest : public InProcessBrowserTest {
 protected:
  DictationExtensionTest() {}
  ~DictationExtensionTest() override = default;
  DictationExtensionTest(const DictationExtensionTest&) = delete;
  DictationExtensionTest& operator=(const DictationExtensionTest&) = delete;

  void SetUp() override {
    fake_speech_recognition_manager_ =
        std::make_unique<content::FakeSpeechRecognitionManager>();
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
    ASSERT_FALSE(AccessibilityManager::Get()->IsDictationEnabled());
    console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
        browser()->profile(), extension_misc::kAccessibilityCommonExtensionId);
    browser()->profile()->GetPrefs()->SetBoolean(
        ash::prefs::kDictationAcceleratorDialogHasBeenAccepted, true);

    extensions::ExtensionHostTestHelper host_helper(
        browser()->profile(), extension_misc::kAccessibilityCommonExtensionId);
    AccessibilityManager::Get()->SetDictationEnabled(true);
    host_helper.WaitForHostCompletedFirstLoad();

    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        GURL(
            "data:text/html;charset=utf-8,<textarea id=textarea></textarea>")));
    // Put focus in the text box.
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, ui::KeyboardCode::VKEY_TAB, false, false, false, false)));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilityDictationExtension);
  }

  void WaitForRecognitionStarted() {
    if (!fake_speech_recognition_manager_->is_recognizing())
      fake_speech_recognition_manager_->WaitForRecognitionStarted();
  }

  void WaitForRecognitionEnded() {
    if (fake_speech_recognition_manager_->is_recognizing())
      fake_speech_recognition_manager_->WaitForRecognitionEnded();

    // Wait for mojom / callbacks to propagate.
    base::RunLoop().RunUntilIdle();
  }

  void SendFinalSpeechResult(const std::string& result) {
    // FakeSpeechRecognitionManager can only send final results.
    // TODO(crbug.com/1216111): Use a MockIMEInputContextHandler to check
    // composition after supporting interim results.
    base::RunLoop loop;
    fake_speech_recognition_manager_->SetFakeResult(result);
    fake_speech_recognition_manager_->SendFakeResponse(
        /*end_recognition=*/false, loop.QuitClosure());
    loop.Run();
  }

  void SendFinalSpeechResultAndWaitForTextAreaValue(const std::string& result,
                                                    const std::string& value) {
    SendFinalSpeechResult(result);
    WaitForTextAreaValue(value);
  }

  std::string GetTextareaValue() {
    std::string output;
    std::string script =
        "window.domAutomationController.send("
        "document.getElementById('textarea').value)";
    CHECK(ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetWebContentsAt(0), script, &output));
    return output;
  }

  void WaitForTextAreaValue(const std::string& value) {
    TextMatchesWaiter waiter(
        value, base::BindRepeating(&DictationExtensionTest::GetTextareaValue,
                                   base::Unretained(this)));
    waiter.Wait();
    base::RunLoop().RunUntilIdle();
  }

  void ToggleDictationWithKeystroke() {
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, ui::KeyboardCode::VKEY_D, false, false, false, true)));
  }

 private:
  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
  std::unique_ptr<content::FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;
};

IN_PROC_BROWSER_TEST_F(DictationExtensionTest, StartsAndStopsRecognition) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  ToggleDictationWithKeystroke();
  WaitForRecognitionEnded();
}

IN_PROC_BROWSER_TEST_F(DictationExtensionTest, EntersFinalizedSpeech) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalSpeechResultAndWaitForTextAreaValue(kFinalSpeechResult,
                                               kFinalSpeechResult);
  ToggleDictationWithKeystroke();
  WaitForRecognitionEnded();
}

IN_PROC_BROWSER_TEST_F(DictationExtensionTest, EntersMultipleFinalizedStrings) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalSpeechResultAndWaitForTextAreaValue("The rain in Spain",
                                               "The rain in Spain");
  SendFinalSpeechResultAndWaitForTextAreaValue(
      " falls mainly on the plain.",
      "The rain in Spain falls mainly on the plain.");
  ToggleDictationWithKeystroke();
  WaitForRecognitionEnded();
}

IN_PROC_BROWSER_TEST_F(DictationExtensionTest,
                       RecognitionEndsWhenInputFieldLosesFocus) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalSpeechResultAndWaitForTextAreaValue("Vega is a star",
                                               "Vega is a star");
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::KeyboardCode::VKEY_TAB, false, false, false, false)));
  WaitForRecognitionEnded();
  EXPECT_EQ("Vega is a star", GetTextareaValue());
}

// Without the feature flag kExperimentalAccessibilityDictationCommands,
// commands should be treated like any other text.
IN_PROC_BROWSER_TEST_F(DictationExtensionTest, IgnoresCommands) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  std::string expected_text = "";
  for (const char* command : kEnglishDictationCommands) {
    expected_text += command;
    SendFinalSpeechResultAndWaitForTextAreaValue(command, expected_text);
  }
  ToggleDictationWithKeystroke();
  WaitForRecognitionEnded();
}

class CaretBoundsChangedWaiter : public ui::InputMethodObserver {
 public:
  explicit CaretBoundsChangedWaiter(ui::InputMethod* input_method)
      : input_method_(input_method) {
    input_method_->AddObserver(this);
  }
  CaretBoundsChangedWaiter(const CaretBoundsChangedWaiter&) = delete;
  CaretBoundsChangedWaiter& operator=(const CaretBoundsChangedWaiter&) = delete;
  ~CaretBoundsChangedWaiter() override { input_method_->RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

 private:
  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {}
  void OnShowVirtualKeyboardIfEnabled() override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {
    run_loop_.Quit();
  }

  ui::InputMethod* input_method_;
  base::RunLoop run_loop_;
};

class ClipboardChangedWaiter : public ui::ClipboardObserver {
 public:
  ClipboardChangedWaiter() {
    ui::ClipboardMonitor::GetInstance()->AddObserver(this);
  }
  ClipboardChangedWaiter(const ClipboardChangedWaiter&) = delete;
  ClipboardChangedWaiter& operator=(const ClipboardChangedWaiter&) = delete;
  ~ClipboardChangedWaiter() override {
    ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

 private:
  // ui::ClipboardObserver:
  void OnClipboardDataChanged() override { run_loop_.Quit(); }

  base::RunLoop run_loop_;
};

class DictationCommandsExtensionTest : public DictationExtensionTest {
 protected:
  DictationCommandsExtensionTest() {}
  ~DictationCommandsExtensionTest() override = default;
  DictationCommandsExtensionTest(const DictationCommandsExtensionTest&) =
      delete;
  DictationCommandsExtensionTest& operator=(
      const DictationCommandsExtensionTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DictationExtensionTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kExperimentalAccessibilityDictationCommands);
  }

  void SetUpOnMainThread() override {
    DictationExtensionTest::SetUpOnMainThread();
    ToggleDictationWithKeystroke();
    WaitForRecognitionStarted();
  }

  void TearDownOnMainThread() override {
    ToggleDictationWithKeystroke();
    WaitForRecognitionEnded();
    DictationExtensionTest::TearDownOnMainThread();
  }

  void WaitForCaretBoundsChanged() {
    CaretBoundsChangedWaiter waiter(
        browser()->window()->GetNativeWindow()->GetHost()->GetInputMethod());
    waiter.Wait();
  }

  void WaitForClipboardDataChanged() {
    ClipboardChangedWaiter waiter;
    waiter.Wait();
  }

  std::string GetClipboardText() {
    std::u16string text;
    ui::Clipboard::GetForCurrentThread()->ReadText(
        ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &text);
    return base::UTF16ToUTF8(text);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationCommandsExtensionTest, TypesCommands) {
  std::string expected_text = "";
  for (const char* command : kEnglishDictationCommands) {
    std::string type_command = "type ";
    expected_text += command;
    SendFinalSpeechResultAndWaitForTextAreaValue(type_command + command,
                                                 expected_text);
  }
}

IN_PROC_BROWSER_TEST_F(DictationCommandsExtensionTest, DeleteCharacter) {
  SendFinalSpeechResultAndWaitForTextAreaValue("Vega", "Vega");

  // Capitalization and whitespace shouldn't matter.
  SendFinalSpeechResultAndWaitForTextAreaValue(" Delete", "Veg");
  SendFinalSpeechResultAndWaitForTextAreaValue("delete ", "Ve");
  SendFinalSpeechResultAndWaitForTextAreaValue("  delete ", "V");
  SendFinalSpeechResultAndWaitForTextAreaValue("DELETE", "");
}

IN_PROC_BROWSER_TEST_F(DictationCommandsExtensionTest, MoveByCharacter) {
  SendFinalSpeechResultAndWaitForTextAreaValue("Lyra", "Lyra");

  SendFinalSpeechResult("Move left");
  WaitForCaretBoundsChanged();
  SendFinalSpeechResultAndWaitForTextAreaValue(" inserted ", "Lyr inserted a");
  SendFinalSpeechResult("move Right ");
  WaitForCaretBoundsChanged();
  SendFinalSpeechResultAndWaitForTextAreaValue(
      " is a constellation", "Lyr inserted a is a constellation");
}

IN_PROC_BROWSER_TEST_F(DictationCommandsExtensionTest, NewLineAndMoveByLine) {
  SendFinalSpeechResultAndWaitForTextAreaValue("Line 1", "Line 1");

  SendFinalSpeechResultAndWaitForTextAreaValue("new line", "Line 1\n");

  SendFinalSpeechResultAndWaitForTextAreaValue("Line 2", "Line 1\nLine 2");

  SendFinalSpeechResult("Move up");
  WaitForCaretBoundsChanged();
  SendFinalSpeechResultAndWaitForTextAreaValue("up", "Line 1up\nLine 2");

  SendFinalSpeechResult("Move down");
  WaitForCaretBoundsChanged();
  SendFinalSpeechResultAndWaitForTextAreaValue("down", "Line 1up\nLine 2down");
}

IN_PROC_BROWSER_TEST_F(DictationCommandsExtensionTest, UndoAndRedo) {
  SendFinalSpeechResultAndWaitForTextAreaValue("The constellation",
                                               "The constellation");
  SendFinalSpeechResultAndWaitForTextAreaValue(" Myra",
                                               "The constellation Myra");
  SendFinalSpeechResultAndWaitForTextAreaValue("undo", "The constellation");
  SendFinalSpeechResultAndWaitForTextAreaValue(" Lyra",
                                               "The constellation Lyra");
  SendFinalSpeechResultAndWaitForTextAreaValue("undo", "The constellation");
  SendFinalSpeechResultAndWaitForTextAreaValue("redo",
                                               "The constellation Lyra");
}

IN_PROC_BROWSER_TEST_F(DictationCommandsExtensionTest, SelectAndUnselectAll) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SendFinalSpeechResultAndWaitForTextAreaValue(
      "Vega is the brightest star in Lyra",
      "Vega is the brightest star in Lyra");
  SendFinalSpeechResult("Select all");
  WaitForSelectionBoundingBoxUpdate(web_contents);
  SendFinalSpeechResultAndWaitForTextAreaValue("delete", "");

  SendFinalSpeechResultAndWaitForTextAreaValue(
      "Vega is the fifth brightest star in the sky",
      "Vega is the fifth brightest star in the sky");
  SendFinalSpeechResult("Select all");
  WaitForSelectionBoundingBoxUpdate(web_contents);
  SendFinalSpeechResult("Unselect all");
  WaitForSelectionBoundingBoxUpdate(web_contents);
  SendFinalSpeechResultAndWaitForTextAreaValue(
      "!", "Vega is the fifth brightest star in the sky!");
}

IN_PROC_BROWSER_TEST_F(DictationCommandsExtensionTest, CutCopyPaste) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SendFinalSpeechResultAndWaitForTextAreaValue("Star", "Star");
  SendFinalSpeechResult("Select all");
  WaitForSelectionBoundingBoxUpdate(web_contents);

  SendFinalSpeechResult("Copy");
  WaitForClipboardDataChanged();
  EXPECT_EQ("Star", GetClipboardText());
  SendFinalSpeechResult("unselect all");
  WaitForSelectionBoundingBoxUpdate(web_contents);

  SendFinalSpeechResultAndWaitForTextAreaValue("paste", "StarStar");

  SendFinalSpeechResult("select ALL ");
  WaitForSelectionBoundingBoxUpdate(web_contents);
  SendFinalSpeechResult("cut");
  WaitForClipboardDataChanged();
  EXPECT_EQ("StarStar", GetClipboardText());
  WaitForTextAreaValue("");

  SendFinalSpeechResultAndWaitForTextAreaValue("  PaStE ", "StarStar");
}

}  // namespace ash
