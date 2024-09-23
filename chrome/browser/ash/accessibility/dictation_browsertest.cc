// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/public/cpp/test/accessibility_controller_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/autoclick_test_utils.h"
#include "chrome/browser/ash/accessibility/automation_test_utils.h"
#include "chrome/browser/ash/accessibility/dictation_bubble_test_helper.h"
#include "chrome/browser/ash/accessibility/dictation_test_utils.h"
#include "chrome/browser/ash/accessibility/select_to_speak_test_utils.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/ash/accessibility/switch_access_test_utils.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/api/audio/audio_api.h"
#include "extensions/browser/api/audio/audio_service.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/events/test/event_generator.h"

namespace ash {

using EditableType = DictationTestUtils::EditableType;

namespace {

const char kFirstSpeechResult[] = "Help";
const char16_t kFirstSpeechResult16[] = u"Help";
const char kFinalSpeechResult[] = "Hello world";
const char16_t kFinalSpeechResult16[] = u"Hello world";
const char16_t kTrySaying[] = u"Try saying:";
const char16_t kType[] = u"\"Type [word / phrase]\"";
const char16_t kHelp[] = u"\"Help\"";
const char16_t kUndo[] = u"\"Undo\"";
const char16_t kDelete[] = u"\"Delete\"";
const char16_t kSelectAll[] = u"\"Select all\"";
const char16_t kUnselect[] = u"\"Unselect\"";
const char16_t kCopy[] = u"\"Copy\"";
const char* kOnDeviceListeningDurationMetric =
    "Accessibility.CrosDictation.ListeningDuration.OnDeviceRecognition";
const char* kNetworkListeningDurationMetric =
    "Accessibility.CrosDictation.ListeningDuration.NetworkRecognition";
const char* kLocaleMetric = "Accessibility.CrosDictation.Language";
const char* kOnDeviceSpeechMetric =
    "Accessibility.CrosDictation.UsedOnDeviceSpeech";
const char* kPumpkinUsedMetric = "Accessibility.CrosDictation.UsedPumpkin";
const char* kPumpkinSucceededMetric =
    "Accessibility.CrosDictation.PumpkinSucceeded";
const char* kMacroRecognizedMetric =
    "Accessibility.CrosDictation.MacroRecognized";
const char* kMacroSucceededMetric =
    "Accessibility.CrosDictation.MacroSucceeded";
const char* kMacroFailedMetric = "Accessibility.CrosDictation.MacroFailed";
const int kInputTextViewMetricValue = 1;

static const char* kEnglishDictationCommands[] = {
    "delete",
    "move to the previous character",
    "move to the next character",
    "move to the previous line",
    "move to the next line",
    "copy",
    "paste",
    "cut",
    "undo",
    "redo",
    "select all",
    "unselect",
    "help",
    "new line",
    "cancel",
    "delete the previous word",
    "delete the previous sentence",
    "move to the next word",
    "move to the previous word",
    "delete phrase",
    "replace phrase with another phrase",
    "insert phrase before another phrase",
    "select from phrase to another phrase",
    "move to the next sentence",
    "move to the previous sentence"};

constexpr auto kOfflineNotSupportedLocaleSet =
    base::MakeFixedFlatSet<std::string_view>({"af-ZA", "kn-IN"});

PrefService* GetActiveUserPrefs() {
  return ProfileManager::GetActiveUserProfile()->GetPrefs();
}

AccessibilityManager* GetManager() {
  return AccessibilityManager::Get();
}

void EnableChromeVox() {
  GetManager()->EnableSpokenFeedback(true);
}

// A class used to define the parameters of a test case.
class TestConfig {
 public:
  TestConfig(speech::SpeechRecognitionType speech_recognition_type,
             EditableType editable_type)
      : speech_recognition_type_(speech_recognition_type),
        editable_type_(editable_type) {}

  speech::SpeechRecognitionType speech_recognition_type() const {
    return speech_recognition_type_;
  }

  EditableType editable_type() const { return editable_type_; }

 private:
  speech::SpeechRecognitionType speech_recognition_type_;
  EditableType editable_type_;
};

}  // namespace

class DictationTestBase : public AccessibilityFeatureBrowserTest,
                          public ::testing::WithParamInterface<TestConfig> {
 public:
  DictationTestBase() = default;
  ~DictationTestBase() override = default;
  DictationTestBase(const DictationTestBase&) = delete;
  DictationTestBase& operator=(const DictationTestBase&) = delete;

 protected:
  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    utils_ = std::make_unique<DictationTestUtils>(speech_recognition_type(),
                                                  editable_type());

    std::vector<base::test::FeatureRef> enabled_features =
        utils_->GetEnabledFeatures();
    std::vector<base::test::FeatureRef> disabled_features =
        utils_->GetDisabledFeatures();
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    utils_->EnableDictation(
        /*profile=*/GetProfile(),
        /*navigate_to_url=*/base::BindOnce(&DictationTestBase::NavigateToUrl,
                                           base::Unretained(this)));
  }

  // Routers to DictationTestUtils methods.
  void WaitForRecognitionStarted() { utils_->WaitForRecognitionStarted(); }

  void WaitForRecognitionStopped() { utils_->WaitForRecognitionStopped(); }

  void SendInterimResultAndWait(const std::string& transcript) {
    utils_->SendInterimResultAndWait(transcript);
  }

  void SendFinalResultAndWait(const std::string& transcript) {
    utils_->SendFinalResultAndWait(transcript);
  }

  void SendErrorAndWait() { utils_->SendErrorAndWait(); }

  void SendFinalResultAndWaitForEditableValue(const std::string& result,
                                              const std::string& value) {
    utils_->SendFinalResultAndWaitForEditableValue(result, value);
  }

  void SendFinalResultAndWaitForSelection(const std::string& result,
                                          int start,
                                          int end) {
    utils_->SendFinalResultAndWaitForSelection(result, start, end);
  }

  void SendFinalResultAndWaitForClipboardChanged(const std::string& result) {
    utils_->SendFinalResultAndWaitForClipboardChanged(result);
  }

  std::string GetEditableValue() { return utils_->GetEditableValue(); }

  void WaitForEditableValue(const std::string& value) {
    utils_->WaitForEditableValue(value);
  }

  void ToggleDictationWithKeystroke() {
    utils_->ToggleDictationWithKeystroke();
  }

  void InstallMockInputContextHandler() {
    utils_->InstallMockInputContextHandler();
  }

  // Retrieves the number of times commit text is updated.
  int GetCommitTextCallCount() { return utils_->GetCommitTextCallCount(); }

  void WaitForCommitText(const std::u16string& value) {
    utils_->WaitForCommitText(value);
  }

  const base::flat_map<std::string, Dictation::LocaleData>
  GetAllSupportedLocales() {
    return Dictation::GetAllSupportedLocales();
  }

  void DisablePumpkin() { utils_->DisablePumpkin(); }

  void ExecuteAccessibilityCommonScript(const std::string& script) {
    utils_->ExecuteAccessibilityCommonScript(script);
  }

  std::string GetClipboardText() {
    std::u16string text;
    ui::Clipboard::GetForCurrentThread()->ReadText(
        ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &text);
    return base::UTF16ToUTF8(text);
  }

  void PressTab() {
    ui::test::EmulateFullKeyPressReleaseSequence(
        /*generator=*/generator(),
        /*key=*/ui::KeyboardCode::VKEY_TAB,
        /*control=*/false,
        /*shift=*/false,
        /*alt=*/false,
        /*command=*/false);
  }

  bool RunOnMultilineContent() {
    // TODO(b:259353252): Contenteditables have an error where inserting a \n
    // actually inserts two \n characters.
    // <input> represents a one-line plain text control, so multiline test cases
    // should be skipped. Run multiline test cases only on <textarea> for these
    // reasons.
    return editable_type() == EditableType::kTextArea;
  }

  speech::SpeechRecognitionType speech_recognition_type() {
    return GetParam().speech_recognition_type();
  }

  EditableType editable_type() { return GetParam().editable_type(); }
  ui::test::EventGenerator* generator() { return utils_->generator(); }

  void set_wait_for_accessibility_common_extension_load_(bool use) {
    utils_->set_wait_for_accessibility_common_extension_load_(use);
  }

  DictationTestUtils* utils() { return utils_.get(); }
 private:
  std::unique_ptr<DictationTestUtils> utils_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DictationTest : public DictationTestBase {
 public:
  DictationTest() = default;
  ~DictationTest() override = default;
  DictationTest(const DictationTest&) = delete;
  DictationTest& operator=(const DictationTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    GetActiveUserPrefs()->SetString(prefs::kAccessibilityDictationLocale,
                                    "en-US");
    DictationTestBase::SetUpOnMainThread();
  }
};

INSTANTIATE_TEST_SUITE_P(
    NetworkContentEditable,
    DictationTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kContentEditable)));

INSTANTIATE_TEST_SUITE_P(
    OnDeviceTextArea,
    DictationTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kOnDevice,
                                 EditableType::kTextArea)));

// Tests the behavior of the GetAllSupportedLocales method, specifically how
// it sets locale data.
IN_PROC_BROWSER_TEST_P(DictationTest, GetAllSupportedLocales) {
  auto locales = GetAllSupportedLocales();
  for (auto& it : locales) {
    const std::string locale = it.first;
    bool works_offline = it.second.works_offline;
    bool installed = it.second.installed;
    if (speech_recognition_type() == speech::SpeechRecognitionType::kOnDevice &&
        locale == speech::kUsEnglishLocale) {
      // We are certain that en_US works offline, so that must be true. There
      // are others too (depending on flags) but we can't test them.  We can be
      // certain that we haven't installed them though.
      EXPECT_TRUE(works_offline);
      EXPECT_TRUE(installed);
    } else {
      EXPECT_FALSE(installed) << " for locale " << locale;
      if (base::Contains(kOfflineNotSupportedLocaleSet, locale)) {
        EXPECT_FALSE(works_offline) << " for locale " << locale;
      }
    }
  }

  if (speech_recognition_type() == speech::SpeechRecognitionType::kOnDevice) {
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
      // We are certain that en_US works offline, so that must be true. There
      // are others too (depending on flags) but we can't test them.  We can be
      // certain that we haven't installed them though.
      EXPECT_TRUE(works_offline);
      EXPECT_FALSE(installed);
    } else {
      EXPECT_FALSE(installed) << " for locale " << locale;
      if (base::Contains(kOfflineNotSupportedLocaleSet, locale)) {
        EXPECT_FALSE(works_offline) << " for locale " << locale;
      }
    }
  }
}

IN_PROC_BROWSER_TEST_P(DictationTest, StartsAndStopsRecognition) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationTest, EntersFinalizedSpeech) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue(kFinalSpeechResult,
                                         kFinalSpeechResult);
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

// Tests that multiple finalized strings can be committed to the text area.
// Also ensures that spaces are added between finalized utterances.
IN_PROC_BROWSER_TEST_P(DictationTest, EntersMultipleFinalizedStrings) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue("The rain in Spain",
                                         "The rain in Spain");
  SendFinalResultAndWaitForEditableValue(
      "falls mainly on the plain.",
      "The rain in Spain falls mainly on the plain.");
  SendFinalResultAndWaitForEditableValue(
      "Vega is a star.",
      "The rain in Spain falls mainly on the plain. Vega is a star.");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationTest, OnlyAddSpaceWhenNecessary) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue("The rain in Spain",
                                         "The rain in Spain");
  // Artificially add a space to this utterance. Verify that only one space is
  // added.
  SendFinalResultAndWaitForEditableValue(
      " falls mainly on the plain.",
      "The rain in Spain falls mainly on the plain.");
  // Artificially add a space to this utterance. Verify that only one space is
  // added.
  SendFinalResultAndWaitForEditableValue(
      " Vega is a star.",
      "The rain in Spain falls mainly on the plain. Vega is a star.");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationTest, RecognitionEndsWhenInputFieldLosesFocus) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue("Vega is a star", "Vega is a star");
  PressTab();
  WaitForRecognitionStopped();
  EXPECT_EQ("Vega is a star", GetEditableValue());
}

IN_PROC_BROWSER_TEST_P(DictationTest, UserEndsDictationWhenChromeVoxEnabled) {
  EnableChromeVox();
  EXPECT_TRUE(GetManager()->IsSpokenFeedbackEnabled());
  InstallMockInputContextHandler();

  GetManager()->ToggleDictation();
  WaitForRecognitionStarted();
  SendInterimResultAndWait(kFinalSpeechResult);
  GetManager()->ToggleDictation();
  WaitForRecognitionStopped();

  WaitForCommitText(kFinalSpeechResult16);
}

IN_PROC_BROWSER_TEST_P(DictationTest, ChromeVoxSilencedWhenToggledOn) {
  // Set up ChromeVox.
  test::SpeechMonitor sm;
  EXPECT_FALSE(GetManager()->IsSpokenFeedbackEnabled());
  extensions::ExtensionHostTestHelper host_helper(
      GetProfile(), extension_misc::kChromeVoxExtensionId);
  EnableChromeVox();
  host_helper.WaitForHostCompletedFirstLoad();
  EXPECT_TRUE(GetManager()->IsSpokenFeedbackEnabled());

  // Not yet forced to stop.
  EXPECT_EQ(0, sm.stop_count());

  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();

  // Assert ChromeVox was asked to stop speaking at the toggle. Note: multiple
  // requests to stop speech can be sent, so we just expect stop_count() > 0.
  EXPECT_GT(sm.stop_count(), 0);
}

IN_PROC_BROWSER_TEST_P(DictationTest, WorksWithSelectToSpeak) {
  // Set up Select to Speak.
  test::SpeechMonitor sm;
  EXPECT_FALSE(GetManager()->IsSelectToSpeakEnabled());
  sts_test_utils::TurnOnSelectToSpeakForTest(
      AccessibilityManager::Get()->profile());

  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue(
      "Not idly do the leaves of Lorien fall",
      "Not idly do the leaves of Lorien fall");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();

  aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root_window);

  gfx::Rect bounds =
      utils()->automation_test_utils()->GetBoundsForNodeInRootByClassName(
          "editableForDictation");
  sts_test_utils::StartSelectToSpeakWithBounds(bounds, &generator);

  // Now ensure STS still works properly.
  sm.ExpectSpeechPattern("Not idly do the leaves of Lorien fall*");
  sm.Replay();
}

IN_PROC_BROWSER_TEST_P(DictationTest, EntersInterimSpeechWhenToggledOff) {
  InstallMockInputContextHandler();

  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendInterimResultAndWait(kFirstSpeechResult);
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
  WaitForCommitText(kFirstSpeechResult16);
}

// Tests that commit text is not updated if the user toggles dictation and no
// speech results are processed.
IN_PROC_BROWSER_TEST_P(DictationTest, UserEndsDictationBeforeSpeech) {
  InstallMockInputContextHandler();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
  EXPECT_EQ(0, GetCommitTextCallCount());
}

// Ensures that the correct metrics are recorded when Dictation is toggled.
IN_PROC_BROWSER_TEST_P(DictationTest, Metrics) {
  base::HistogramTester histogram_tester_;
  bool on_device =
      speech_recognition_type() == speech::SpeechRecognitionType::kOnDevice;
  const char* metric_name = on_device ? kOnDeviceListeningDurationMetric
                                      : kNetworkListeningDurationMetric;
  HistogramWaiter waiter(metric_name);
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
  waiter.Wait();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Ensure that we recorded the correct locale.
  histogram_tester_.ExpectUniqueSample(/*name=*/kLocaleMetric,
                                       /*sample=*/base::HashMetricName("en-US"),
                                       /*expected_bucket_count=*/1);
  // Ensure that we recorded the type of speech recognition and listening
  // duration.
  if (on_device) {
    histogram_tester_.ExpectUniqueSample(/*name=*/kOnDeviceSpeechMetric,
                                         /*sample=*/true,
                                         /*expected_bucket_count=*/1);
    ASSERT_EQ(1u,
              histogram_tester_.GetAllSamples(kOnDeviceListeningDurationMetric)
                  .size());
    // Ensure there are no metrics for the other type of speech recognition.
    ASSERT_EQ(0u,
              histogram_tester_.GetAllSamples(kNetworkListeningDurationMetric)
                  .size());
  } else {
    histogram_tester_.ExpectUniqueSample(/*name=*/kOnDeviceSpeechMetric,
                                         /*sample=*/false,
                                         /*expected_bucket_count=*/1);
    ASSERT_EQ(1u,
              histogram_tester_.GetAllSamples(kNetworkListeningDurationMetric)
                  .size());
    // Ensure there are no metrics for the other type of speech recognition.
    ASSERT_EQ(0u,
              histogram_tester_.GetAllSamples(kOnDeviceListeningDurationMetric)
                  .size());
  }
}

IN_PROC_BROWSER_TEST_P(DictationTest,
                       DictationStopsWhenSystemTrayBecomesVisible) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SystemTrayTestApi::Create()->ShowBubble();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationTest, NoExtraSpaceForPunctuation) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue("Hello world", "Hello world");
  SendFinalResultAndWaitForEditableValue(".", "Hello world.");
  SendFinalResultAndWaitForEditableValue("Goodnight", "Hello world. Goodnight");
  SendFinalResultAndWaitForEditableValue("!", "Hello world. Goodnight!");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationTest, StopListening) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWait("cancel");
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationTest, SmartCapitalization) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue("this", "This");
  SendFinalResultAndWaitForEditableValue("Is", "This is");
  SendFinalResultAndWaitForEditableValue("a test.", "This is a test.");
  SendFinalResultAndWaitForEditableValue("you passed!",
                                         "This is a test. You passed!");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationTest, SmartCapitalizationWithComma) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue("Hello,", "Hello,");
  SendFinalResultAndWaitForEditableValue("world", "Hello, world");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

// Note: this test runs the SMART_DELETE_PHRASE macro and at first glance
// should be categorized as a DictationRegexCommandsTest. However, this test
// stops speech recognition in the middle of the test, which directly conflicts
// with DictationRegexCommandsTest's behavior to automatically stop speech
// recognition during teardown. Thus we need this to be a DictationTest so that
// we don't try to stop speech recognition when it's already been stopped.
IN_PROC_BROWSER_TEST_P(DictationTest, SmartDeletePhraseNoChange) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue("Hello world", "Hello world");
  SendFinalResultAndWait("delete banana");
  SendFinalResultAndWait("cancel");
  WaitForRecognitionStopped();
  // The text area value isn't changed because Dictation parses 'delete banana'
  // as the SMART_DELETE_PHRASE macro. Since there is no instance of 'banana' in
  // the text field, the macro does nothing and the text area value remains
  // unchanged. In the future, we can verify that context-checking failed and an
  // error was surfaced to the user.
  ASSERT_EQ("Hello world", GetEditableValue());
}

// Is a DictationTest for the same reason as the above test.
IN_PROC_BROWSER_TEST_P(DictationTest, Help) {
  // Setup a TestNavigationObserver, which will allow us to know when the help
  // center URL is loaded.
  auto observer = std::make_unique<content::TestNavigationObserver>(
      GURL("https://support.google.com/chromebook?p=text_dictation_m100"));
  observer->WatchExistingWebContents();
  observer->StartWatchingNewWebContents();

  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWait("help");

  // Speech recognition should automatically turn off.
  WaitForRecognitionStopped();
  // Wait for the help center URL to load.
  observer->Wait();
}

// Confirms that punctuation can be sent and entered into the editable. We
// unfortuantely can't test that "period" -> "." or "exclamation point" -> "!"
// since that computation happens in the speech recognition engine.
IN_PROC_BROWSER_TEST_P(DictationTest, Punctuation) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  std::string text = "Testing Dictation. It's a great feature!";
  SendFinalResultAndWaitForEditableValue(text, text);
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationTest,
                       TogglesOnIfSodaDownloadingInDifferentLanguage) {
  if (speech_recognition_type() != speech::SpeechRecognitionType::kOnDevice) {
    // SodaInstaller only works if on-device speech recognition is available.
    return;
  }

  speech::SodaInstaller::GetInstance()->NotifySodaProgressForTesting(
      30, speech::LanguageCode::kFrFr);
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

// Verifies that Dictation cannot be toggled on using the keyboard shortcut if
// a SODA download is in-progress.
IN_PROC_BROWSER_TEST_P(DictationTest,
                       NoToggleOnIfSodaDownloadingInDictationLanguage) {
  if (speech_recognition_type() != speech::SpeechRecognitionType::kOnDevice) {
    // SodaInstaller only works if on-device speech recognition is available.
    return;
  }

  // Dictation shouldn't work if SODA is downloading in the Dictation locale.
  speech::SodaInstaller::GetInstance()->NotifySodaProgressForTesting(
      30, speech::LanguageCode::kEnUs);
  ExecuteAccessibilityCommonScript(
      "dictationTestSupport.installFakeSpeechRecognitionPrivateStart();");
  ToggleDictationWithKeystroke();
  // Sanity check that speech recognition is off and that no calls to
  // chrome.speechRecognitionPrivate.start() were made.
  WaitForRecognitionStopped();
  ExecuteAccessibilityCommonScript(
      "dictationTestSupport.ensureNoSpeechRecognitionPrivateStartCalls();");
  ExecuteAccessibilityCommonScript(
      "dictationTestSupport.restoreSpeechRecognitionPrivateStart();");

  // Dictation should work again once the SODA download is finished.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

class DictationWithAutoclickTest : public DictationTestBase {
 public:
  DictationWithAutoclickTest() = default;
  ~DictationWithAutoclickTest() override = default;
  DictationWithAutoclickTest(const DictationWithAutoclickTest&) = delete;
  DictationWithAutoclickTest& operator=(const DictationWithAutoclickTest&) =
      delete;

 protected:
  void SetUpOnMainThread() override {
    // Setup Autoclick first, then setup Dictation. This is to ensure that
    // Autoclick doesn't steal focus away from the textarea (either by clicking
    // or via the presence of the Autoclick UI, which steals focus when
    // initially shown).
    autoclick_test_utils_ = std::make_unique<AutoclickTestUtils>(GetProfile());
    autoclick_test_utils_->SetAutoclickDelayMs(90 * 1000);
    // Don't install automation test utils JS, Dictation already does this.
    autoclick_test_utils_->LoadAutoclick(/*install_automation_utils=*/false);
    EXPECT_TRUE(GetManager()->IsAutoclickEnabled());
    // Don't use ExtensionHostTestHelper for Dictation because we already used
    // it for Autoclick.
    set_wait_for_accessibility_common_extension_load_(false);
    DictationTestBase::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { autoclick_test_utils_.reset(); }

  AutoclickTestUtils* autoclick_test_utils() {
    return autoclick_test_utils_.get();
  }

 private:
  std::unique_ptr<AutoclickTestUtils> autoclick_test_utils_;
};

INSTANTIATE_TEST_SUITE_P(
    NetworkTextArea,
    DictationWithAutoclickTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kTextArea)));

IN_PROC_BROWSER_TEST_P(DictationWithAutoclickTest, UseBothFeatures) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue("Hello world", "Hello world");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();

  autoclick_test_utils()->SetAutoclickEventTypeWithHover(
      generator(), AutoclickEventType::kDoubleClick);
  autoclick_test_utils()->SetAutoclickDelayMs(5);
  // Hovering over the editable should result in the text being selected.
  autoclick_test_utils()->HoverOverHtmlElement(generator(), "Hello world",
                                               "staticText");
  utils()->automation_test_utils()->WaitForTextSelectionChangedEvent();
}

IN_PROC_BROWSER_TEST_P(DictationWithAutoclickTest, UseAutoclickToToggle) {
  autoclick_test_utils()->SetAutoclickEventTypeWithHover(
      generator(), AutoclickEventType::kLeftClick);
  autoclick_test_utils()->SetAutoclickDelayMs(5);
  gfx::Rect dictation_button = Shell::Get()
                                   ->GetPrimaryRootWindowController()
                                   ->GetStatusAreaWidget()
                                   ->dictation_button_tray()
                                   ->GetBoundsInScreen();
  // Move the mouse to the Dictation button.
  generator()->MoveMouseTo(dictation_button.CenterPoint());
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue("Hello world", "Hello world");
  // Move the mouse away from, then back to the Dictation button.
  generator()->MoveMouseTo(gfx::Point(20, 20));
  generator()->MoveMouseTo(dictation_button.CenterPoint());
  WaitForRecognitionStopped();
}

class DictationWithSwitchAccessTest : public DictationTestBase {
 public:
  DictationWithSwitchAccessTest() = default;
  ~DictationWithSwitchAccessTest() override = default;
  DictationWithSwitchAccessTest(const DictationWithSwitchAccessTest&) = delete;
  DictationWithSwitchAccessTest& operator=(
      const DictationWithSwitchAccessTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    switch_access_test_utils_ =
        std::make_unique<SwitchAccessTestUtils>(GetProfile());
    switch_access_test_utils_->EnableSwitchAccess({'1', 'A'} /* select */,
                                                  {'2', 'B'} /* next */,
                                                  {'3', 'C'} /* previous */);
    DictationTestBase::SetUpOnMainThread();
  }

 private:
  std::unique_ptr<SwitchAccessTestUtils> switch_access_test_utils_;
};

INSTANTIATE_TEST_SUITE_P(
    NetworkTextArea,
    DictationWithSwitchAccessTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kTextArea)));

IN_PROC_BROWSER_TEST_P(DictationWithSwitchAccessTest, CanDictate) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue("Hello", "Hello");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

// Tests the behavior of Dictation in Japanese.
class DictationJaTest : public DictationTestBase {
 public:
  DictationJaTest() = default;
  ~DictationJaTest() override = default;
  DictationJaTest(const DictationJaTest&) = delete;
  DictationJaTest& operator=(const DictationJaTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    locale_util::SwitchLanguage("ja", /*enable_locale_keyboard_layouts=*/true,
                                /*login_layouts_only*/ false, base::DoNothing(),
                                GetProfile());
    g_browser_process->SetApplicationLocale("ja");
    GetActiveUserPrefs()->SetString(prefs::kAccessibilityDictationLocale, "ja");

    DictationTestBase::SetUpOnMainThread();

    DisablePumpkin();
  }
};

// On-device speech recognition is currently limited to en-US, so
// DictationJaTest should use network speech recognition only.
INSTANTIATE_TEST_SUITE_P(
    NetworkTextArea,
    DictationJaTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kTextArea)));

INSTANTIATE_TEST_SUITE_P(
    NetworkContentEditable,
    DictationJaTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kContentEditable)));

IN_PROC_BROWSER_TEST_P(DictationJaTest, NoSmartSpacingOrCapitalization) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue("this", "this");
  SendFinalResultAndWaitForEditableValue(" Is", "this Is");
  SendFinalResultAndWaitForEditableValue("a test.", "this Isa test.");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationJaTest, CanDictate) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue("テニス", "テニス");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationJaTest, DeleteCharacter) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  // Dictate "tennis".
  SendFinalResultAndWaitForEditableValue("テニス", "テニス");
  // Perform the 'delete' command.
  SendFinalResultAndWaitForEditableValue("削除", "テニ");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationJaTest, SmartDeletePhrase) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  // Dictate "I like basketball".
  SendFinalResultAndWaitForEditableValue("私はバスケットボールが好きです。",
                                         "私はバスケットボールが好きです。");
  // Delete "I" e.g. the first two characters in the sentence.
  SendFinalResultAndWaitForEditableValue("私はを削除",
                                         "バスケットボールが好きです。");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationJaTest, SmartReplacePhrase) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  // Dictate "I like basketball".
  SendFinalResultAndWaitForEditableValue("私はバスケットボールが好きです。",
                                         "私はバスケットボールが好きです。");
  // Replace "basketball" with "tennis".
  SendFinalResultAndWaitForEditableValue("バスケットボールをテニスに置き換え",
                                         "私はテニスが好きです。");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationJaTest, SmartInsertBefore) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  // Dictate "I like tennis".
  SendFinalResultAndWaitForEditableValue("私はテニスが好きです。",
                                         "私はテニスが好きです。");
  // Insert "basketball and" before "tennis".
  // Final text area value should be "I like basketball and tennis".
  SendFinalResultAndWaitForEditableValue(
      "バスケットボールとをテニスの前に挿入",
      "私はバスケットボールとテニスが好きです。");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationJaTest, SmartSelectBetweenAndDictate) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  // Dictate "I like tennis".
  SendFinalResultAndWaitForEditableValue("私はテニスが好きです。",
                                         "私はテニスが好きです。");
  // Select the entire text using the SMART_SELECT_BETWEEN command.
  SendFinalResultAndWaitForSelection("私はから好きですまで選択", 0, 10);
  // Dictate "congratulations", which should replace the selected text.
  SendFinalResultAndWaitForEditableValue("おめでとう", "おめでとう。");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationJaTest, SmartSelectBetweenAndDelete) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  // Dictate "I like tennis".
  SendFinalResultAndWaitForEditableValue("私はテニスが好きです。",
                                         "私はテニスが好きです。");
  // Select between "I" and "tennis" using the SMART_SELECT_BETWEEN command
  // (roughly the first half of the text).
  SendFinalResultAndWaitForSelection("私はからテニスまで選択", 0, 5);
  // Perform the delete command.
  SendFinalResultAndWaitForEditableValue("削除", "が好きです。");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

// Tests Dictation regex-based commands (no Pumpkin).
class DictationRegexCommandsTest : public DictationTest {
 public:
  DictationRegexCommandsTest() = default;
  ~DictationRegexCommandsTest() override = default;
  DictationRegexCommandsTest(const DictationRegexCommandsTest&) = delete;
  DictationRegexCommandsTest& operator=(const DictationRegexCommandsTest&) =
      delete;

 protected:
  void SetUpOnMainThread() override {
    DictationTest::SetUpOnMainThread();
    // Disable Pumpkin so that we use regex parsing.
    DisablePumpkin();
    ToggleDictationWithKeystroke();
    WaitForRecognitionStarted();
  }

  void TearDownOnMainThread() override {
    ToggleDictationWithKeystroke();
    WaitForRecognitionStopped();
    DictationTest::TearDownOnMainThread();
  }
};

INSTANTIATE_TEST_SUITE_P(
    NetworkInput,
    DictationRegexCommandsTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kInput)));

INSTANTIATE_TEST_SUITE_P(
    NetworkContentEditable,
    DictationRegexCommandsTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kContentEditable)));

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, TypesCommands) {
  std::string expected_text = "";
  int i = 0;
  for (const char* command : kEnglishDictationCommands) {
    std::string type_command = "type ";
    if (i == 0) {
      expected_text += command;
      expected_text[0] = base::ToUpperASCII(expected_text[0]);
    } else {
      expected_text += " ";
      expected_text += command;
    }
    SendFinalResultAndWaitForEditableValue(type_command + command,
                                           expected_text);
    ++i;
  }
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, TypesNonCommands) {
  // The phrase should be entered without the word "type".
  SendFinalResultAndWaitForEditableValue("Type this is a test",
                                         "This is a test");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, DeleteCharacter) {
  SendFinalResultAndWaitForEditableValue("Vega", "Vega");
  // Capitalization and whitespace shouldn't matter.
  SendFinalResultAndWaitForEditableValue(" Delete", "Veg");
  SendFinalResultAndWaitForEditableValue("delete", "Ve");
  SendFinalResultAndWaitForEditableValue("  delete ", "V");
  SendFinalResultAndWaitForEditableValue(
      "DELETE",
      (editable_type() == EditableType::kContentEditable) ? "\n" : "");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, MoveByCharacter) {
  SendFinalResultAndWaitForEditableValue("Lyra", "Lyra");
  SendFinalResultAndWaitForSelection("Move to the Previous character", 3, 3);
  // White space is added to the text on the left of the text caret, but not
  // to the right of the text caret.
  SendFinalResultAndWaitForEditableValue("inserted", "Lyr inserted a");
  SendFinalResultAndWaitForSelection("move TO the next character ", 14, 14);
  SendFinalResultAndWaitForEditableValue("is a constellation",
                                         "Lyr inserted a is a constellation");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, NewLineAndMoveByLine) {
  if (!RunOnMultilineContent())
    return;

  SendFinalResultAndWaitForEditableValue("Line 1", "Line 1");
  SendFinalResultAndWaitForEditableValue("new line", "Line 1\n");
  SendFinalResultAndWaitForEditableValue("line 2", "Line 1\nline 2");
  SendFinalResultAndWaitForSelection("Move to the previous line ", 6, 6);
  SendFinalResultAndWaitForEditableValue("up", "Line 1 up\nline 2");
  SendFinalResultAndWaitForSelection("Move to the next line", 16, 16);
  SendFinalResultAndWaitForEditableValue("down", "Line 1 up\nline 2 down");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, UndoAndRedo) {
  SendFinalResultAndWaitForEditableValue("The constellation",
                                         "The constellation");
  SendFinalResultAndWaitForEditableValue(" Myra", "The constellation Myra");
  SendFinalResultAndWaitForEditableValue("undo", "The constellation");
  SendFinalResultAndWaitForEditableValue(" Lyra", "The constellation Lyra");
  SendFinalResultAndWaitForEditableValue("undo", "The constellation");
  SendFinalResultAndWaitForEditableValue("redo", "The constellation Lyra");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, SelectAllAndUnselect) {
  std::string first_text = "Vega is the brightest star in Lyra";
  SendFinalResultAndWaitForEditableValue(first_text, first_text);
  SendFinalResultAndWaitForSelection("Select all", 0, first_text.size());
  SendFinalResultAndWaitForEditableValue(
      "delete",
      (editable_type() == EditableType::kContentEditable) ? "\n" : "");
  std::string second_text = "Vega is the fifth brightest star in the sky";
  SendFinalResultAndWaitForEditableValue(second_text, second_text);
  SendFinalResultAndWaitForSelection("Select all", 0, second_text.size());
  SendFinalResultAndWaitForSelection("Unselect", second_text.size(),
                                     second_text.size());
  SendFinalResultAndWaitForEditableValue(
      "!", "Vega is the fifth brightest star in the sky!");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, CutCopyPaste) {
  SendFinalResultAndWaitForEditableValue("Star", "Star");
  SendFinalResultAndWaitForSelection("Select all", 0, 4);
  SendFinalResultAndWaitForClipboardChanged("Copy");
  EXPECT_EQ("Star", GetClipboardText());
  SendFinalResultAndWaitForSelection("unselect", 4, 4);
  SendFinalResultAndWaitForEditableValue("paste", "StarStar");
  SendFinalResultAndWaitForSelection("select ALL ", 0, 8);
  SendFinalResultAndWaitForClipboardChanged("cut");
  EXPECT_EQ("StarStar", GetClipboardText());
  WaitForEditableValue(
      (editable_type() == EditableType::kContentEditable) ? "\n" : "");
  SendFinalResultAndWaitForEditableValue("  PaStE ", "StarStar");
}

// Ensures that a metric is recorded when a macro succeeds.
// TODO(crbug.com/1288964): Add a test to ensure that a metric is recorded when
// a macro fails.
IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, MacroSucceededMetric) {
  base::HistogramTester histogram_tester_;
  SendFinalResultAndWaitForEditableValue("Vega is the brightest star in Lyra",
                                         "Vega is the brightest star in Lyra");
  histogram_tester_.ExpectUniqueSample(/*name=*/kMacroSucceededMetric,
                                       /*sample=*/kInputTextViewMetricValue,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(/*name=*/kMacroFailedMetric,
                                       /*sample=*/kInputTextViewMetricValue,
                                       /*expected_bucket_count=*/0);
  histogram_tester_.ExpectUniqueSample(/*name=*/kMacroRecognizedMetric,
                                       /*sample=*/kInputTextViewMetricValue,
                                       /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, DeletePrevWordSimple) {
  SendFinalResultAndWaitForEditableValue("This is a test", "This is a test");
  SendFinalResultAndWaitForEditableValue("delete the previous word",
                                         "This is a ");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, DeletePrevWordExtraSpace) {
  SendFinalResultAndWaitForEditableValue("This is a test ", "This is a test ");
  SendFinalResultAndWaitForEditableValue("delete the previous word",
                                         "This is a ");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, DeletePrevWordNewLine) {
  if (!RunOnMultilineContent())
    return;

  SendFinalResultAndWaitForEditableValue("This is a test\n\n",
                                         "This is a test\n\n");
  SendFinalResultAndWaitForEditableValue("delete the previous word",
                                         "This is a test\n");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, DeletePrevWordPunctuation) {
  SendFinalResultAndWaitForEditableValue("This.is.a.test. ",
                                         "This.is.a.test. ");
  SendFinalResultAndWaitForEditableValue("delete the previous word",
                                         "This.is.a.test");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, DeletePrevWordMiddleOfWord) {
  SendFinalResultAndWaitForEditableValue("This is a test.", "This is a test.");
  // Move the text caret into the middle of the word "test".
  SendFinalResultAndWaitForSelection("Move to the Previous character", 14, 14);
  SendFinalResultAndWaitForSelection("Move to the Previous character", 13, 13);
  SendFinalResultAndWaitForEditableValue("delete the previous word",
                                         "This is a t.");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, DeletePrevSentSimple) {
  SendFinalResultAndWaitForEditableValue("Hello, world.", "Hello, world.");
  SendFinalResultAndWaitForEditableValue(
      "delete the previous sentence",
      (editable_type() == EditableType::kContentEditable) ? "\n" : "");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, DeletePrevSentWhiteSpace) {
  if (!RunOnMultilineContent())
    return;

  SendFinalResultAndWaitForEditableValue("  \nHello, world.\n  ",
                                         "  \nHello, world.\n  ");
  SendFinalResultAndWaitForEditableValue("delete the previous sentence", "");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, DeletePrevSentPunctuation) {
  SendFinalResultAndWaitForEditableValue(
      "Hello, world! Good afternoon; good evening? Goodnight, world.",
      "Hello, world! Good afternoon; good evening? Goodnight, world.");
  SendFinalResultAndWaitForEditableValue(
      "delete the previous sentence",
      "Hello, world! Good afternoon; good evening?");
  SendFinalResultAndWaitForEditableValue("delete the previous sentence",
                                         "Hello, world! Good afternoon;");
  SendFinalResultAndWaitForEditableValue("delete the previous sentence",
                                         "Hello, world!");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, DeletePrevSentTwoSentences) {
  SendFinalResultAndWaitForEditableValue("Hello, world. Goodnight, world.",
                                         "Hello, world. Goodnight, world.");
  SendFinalResultAndWaitForEditableValue("delete the previous sentence",
                                         "Hello, world.");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest,
                       DeletePrevSentMiddleOfSentence) {
  SendFinalResultAndWaitForEditableValue("Hello, world. Goodnight, world.",
                                         "Hello, world. Goodnight, world.");
  // Move the text caret into the middle of the second sentence.
  SendFinalResultAndWaitForSelection("Move to the Previous character", 30, 30);
  SendFinalResultAndWaitForSelection("Move to the Previous character", 29, 29);
  SendFinalResultAndWaitForEditableValue("delete the previous sentence",
                                         "Hello, world.d.");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, MoveByWord) {
  SendFinalResultAndWaitForEditableValue("This is a quiz", "This is a quiz");
  SendFinalResultAndWaitForSelection("move to the previous word", 10, 10);
  SendFinalResultAndWaitForEditableValue("pop ", "This is a pop quiz");
  SendFinalResultAndWaitForSelection("move to the next word", 18, 18);
  SendFinalResultAndWaitForEditableValue("folks!", "This is a pop quiz folks!");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, SmartDeletePhraseSimple) {
  SendFinalResultAndWaitForEditableValue("This is a difficult test",
                                         "This is a difficult test");
  SendFinalResultAndWaitForEditableValue("delete difficult", "This is a test");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest,
                       SmartDeletePhraseCaseInsensitive) {
  SendFinalResultAndWaitForEditableValue("This is a DIFFICULT test",
                                         "This is a DIFFICULT test");
  SendFinalResultAndWaitForEditableValue("delete difficult", "This is a test");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest,
                       SmartDeletePhraseDuplicateMatches) {
  SendFinalResultAndWaitForEditableValue("The cow jumped over the moon.",
                                         "The cow jumped over the moon.");
  // Deletes the right-most occurrence of "the".
  SendFinalResultAndWaitForEditableValue("delete the",
                                         "The cow jumped over moon.");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest,
                       SmartDeletePhraseDeletesLeftOfCaret) {
  SendFinalResultAndWaitForEditableValue("The cow jumped over the moon.",
                                         "The cow jumped over the moon.");
  SendFinalResultAndWaitForSelection("move to the previous word", 28, 28);
  SendFinalResultAndWaitForSelection("move to the previous word", 24, 24);
  SendFinalResultAndWaitForSelection("move to the previous word", 20, 20);
  SendFinalResultAndWaitForEditableValue("delete the",
                                         "cow jumped over the moon.");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest,
                       SmartDeletePhraseDeletesAtWordBoundaries) {
  SendFinalResultAndWaitForEditableValue("A square is also a rectangle.",
                                         "A square is also a rectangle.");
  // Deletes the first word "a", not the first character "a".
  SendFinalResultAndWaitForEditableValue("delete a",
                                         "A square is also rectangle.");
}

// TODO(crbug.com/40901980): Test is flaky.
IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest,
                       DISABLED_SmartReplacePhrase) {
  SendFinalResultAndWaitForEditableValue("This is a difficult test.",
                                         "This is a difficult test.");
  SendFinalResultAndWaitForEditableValue("replace difficult with simple",
                                         "This is a simple test.");
  SendFinalResultAndWaitForEditableValue("replace is with isn't",
                                         "This isn't a simple test.");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, SmartInsertBefore) {
  SendFinalResultAndWaitForEditableValue("This is a test.", "This is a test.");
  SendFinalResultAndWaitForEditableValue("insert simple before test",
                                         "This is a simple test.");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, SmartSelectBetween) {
  SendFinalResultAndWaitForEditableValue("This is a test.", "This is a test.");
  SendFinalResultAndWaitForSelection("select from this to test", 0, 14);
  SendFinalResultAndWaitForEditableValue("Hello world", "Hello world.");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, MoveBySentence) {
  SendFinalResultAndWaitForEditableValue("Hello world! Goodnight world?",
                                         "Hello world! Goodnight world?");
  SendFinalResultAndWaitForSelection("move to the previous sentence", 12, 12);
  SendFinalResultAndWaitForEditableValue(
      "Good evening.", "Hello world! Good evening. Goodnight world?");
  SendFinalResultAndWaitForSelection("move to the next sentence", 43, 43);
  SendFinalResultAndWaitForEditableValue(
      "Time for a midnight snack",
      "Hello world! Good evening. Goodnight world? Time for a midnight snack");
}

// CursorPosition... tests verify the new cursor position after a command is
// performed. The new cursor position is verified by inserting text after the
// command under test is performed.

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest,
                       CursorPositionDeleteSentence) {
  SendFinalResultAndWaitForEditableValue("First. Second.", "First. Second.");
  SendFinalResultAndWaitForEditableValue("delete the previous sentence",
                                         "First.");
  SendFinalResultAndWaitForEditableValue("Third.", "First. Third.");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest,
                       CursorPositionSmartDeletePhrase) {
  SendFinalResultAndWaitForEditableValue("This is a difficult test",
                                         "This is a difficult test");
  SendFinalResultAndWaitForEditableValue("delete difficult", "This is a test");
  SendFinalResultAndWaitForEditableValue("simple", "This is a simple test");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest,
                       CursorPositionSmartReplacePhrase) {
  SendFinalResultAndWaitForEditableValue("This is a difficult test",
                                         "This is a difficult test");
  SendFinalResultAndWaitForEditableValue("replace difficult with simple",
                                         "This is a simple test");
  SendFinalResultAndWaitForEditableValue("biology",
                                         "This is a simple biology test");
  SendFinalResultAndWaitForEditableValue(
      "and chemistry", "This is a simple biology and chemistry test");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest,
                       CursorPositionSmartInsertBefore) {
  SendFinalResultAndWaitForEditableValue("This is a test", "This is a test");
  SendFinalResultAndWaitForEditableValue("insert simple before test",
                                         "This is a simple test");
  SendFinalResultAndWaitForEditableValue("biology",
                                         "This is a simple biology test");
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest,
                       SmartDeletePhraseLongContent) {
  if (!RunOnMultilineContent())
    return;

  std::string first_sentence_initial = R"(
    The dog (Canis familiaris or Canis lupus familiaris) is a domesticated
    descendant of the wolf.
  )";
  // The same as above, except the second instance of "familiaris" is removed.
  std::string first_sentence_final = R"(
    The dog (Canis familiaris or Canis lupus) is a domesticated
    descendant of the wolf.
  )";
  std::string remaining_text = R"(
    Also called the domestic dog, it is derived from an
    ancient, extinct wolf, and the modern wolf is the dog's nearest living
    relative. The dog was the first species to be domesticated, by
    hunter-gatherers over 15,000 years ago, before the development of
    agriculture. Due to their long association with humans, dogs have expanded
    to a large number of domestic individuals and gained the ability to thrive
    on a starch-rich diet that would be inadequate for other canids.

    The dog has been selectively bred over millennia for various behaviors,
    sensory capabilities, and physical attributes. Dog breeds vary widely in
    shape, size, and color. They perform many roles for humans, such as hunting,
    herding, pulling loads, protection, assisting police and the military,
    companionship, therapy, and aiding disabled people. Over the millennia, dogs
    became uniquely adapted to human behavior, and the human-canine bond has
    been a topic of frequent study. This influence on human society has given
    them the sobriquet of "man's best friend".
  )";

  std::string initial_value = first_sentence_initial + remaining_text;
  std::string final_value = first_sentence_final + remaining_text;
  SendFinalResultAndWaitForEditableValue(initial_value, initial_value);
  SendFinalResultAndWaitForEditableValue("delete familiaris", final_value);
}

IN_PROC_BROWSER_TEST_P(DictationRegexCommandsTest, Metrics) {
  base::HistogramTester histogram_tester_;
  HistogramWaiter waiter(kPumpkinUsedMetric);
  SendFinalResultAndWait("Undo");
  waiter.Wait();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester_.ExpectUniqueSample(/*name=*/kPumpkinUsedMetric,
                                       /*sample=*/false,
                                       /*expected_bucket_count=*/1);
}

// Tests the behavior of the Dictation bubble UI.
class DictationUITest : public DictationTest {
 public:
  DictationUITest() = default;
  ~DictationUITest() override = default;
  DictationUITest(const DictationUITest&) = delete;
  DictationUITest& operator=(const DictationUITest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    DictationTest::SetUpOnMainThread();
    dictation_bubble_test_helper_ =
        std::make_unique<DictationBubbleTestHelper>();
  }

  void TearDownOnMainThread() override {
    dictation_bubble_test_helper_.reset();
    DictationTest::TearDownOnMainThread();
  }

  void WaitForProperties(
      bool visible,
      DictationBubbleIconType icon,
      const std::optional<std::u16string>& text,
      const std::optional<std::vector<std::u16string>>& hints) {
    dictation_bubble_test_helper_->WaitForVisibility(visible);
    dictation_bubble_test_helper_->WaitForVisibleIcon(icon);
    if (text.has_value())
      dictation_bubble_test_helper_->WaitForVisibleText(text.value());
    if (hints.has_value())
      dictation_bubble_test_helper_->WaitForVisibleHints(hints.value());
  }

 private:
  std::unique_ptr<DictationBubbleTestHelper> dictation_bubble_test_helper_;
};

INSTANTIATE_TEST_SUITE_P(
    NetworkTextArea,
    DictationUITest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kTextArea)));

IN_PROC_BROWSER_TEST_P(DictationUITest, ShownWhenSpeechRecognitionStarts) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/std::optional<std::u16string>(),
                    /*hints=*/std::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationUITest, DisplaysInterimSpeechResults) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  // Send an interim speech result.
  SendInterimResultAndWait("Testing");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kHidden,
                    /*text=*/u"Testing",
                    /*hints=*/std::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationUITest, DisplaysMacroSuccess) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  // Perform a command.
  SendFinalResultAndWait("Select all");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kMacroSuccess,
                    /*text=*/u"Select all",
                    /*hints=*/std::optional<std::vector<std::u16string>>());
  // UI should return to standby mode after a timeout.
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/std::u16string(),
                    /*hints=*/std::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationUITest,
                       ResetsToStandbyModeAfterFinalSpeechResult) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/std::optional<std::u16string>(),
                    /*hints=*/std::optional<std::vector<std::u16string>>());
  // Send an interim speech result.
  SendInterimResultAndWait("Testing");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kHidden,
                    /*text=*/u"Testing",
                    /*hints=*/std::optional<std::vector<std::u16string>>());
  // Send a final speech result. UI should return to standby mode.
  SendFinalResultAndWait("Testing 123");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/std::u16string(),
                    /*hints=*/std::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationUITest, HiddenWhenDictationDeactivates) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/std::optional<std::u16string>(),
                    /*hints=*/std::optional<std::vector<std::u16string>>());
  // The UI should be hidden when Dictation deactivates.
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
  WaitForProperties(/*visible=*/false,
                    /*icon=*/DictationBubbleIconType::kHidden,
                    /*text=*/std::u16string(),
                    /*hints=*/std::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationUITest, StandbyHints) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/std::optional<std::u16string>(),
                    /*hints=*/std::optional<std::vector<std::u16string>>());
  // Hints should show up after a few seconds without speech.
  WaitForProperties(
      /*visible=*/true,
      /*icon=*/DictationBubbleIconType::kStandby,
      /*text=*/std::optional<std::u16string>(),
      /*hints=*/std::vector<std::u16string>{kTrySaying, kType, kHelp});
}

// Ensures that Search + D can be used to toggle Dictation when ChromeVox is
// active. Also verifies that ChromeVox announces hints when they are shown in
// the Dictation UI.
IN_PROC_BROWSER_TEST_P(DictationUITest, ChromeVoxAnnouncesHints) {
  // Setup ChromeVox first.
  test::SpeechMonitor sm;
  EXPECT_FALSE(GetManager()->IsSpokenFeedbackEnabled());
  extensions::ExtensionHostTestHelper host_helper(
      GetProfile(), extension_misc::kChromeVoxExtensionId);
  EnableChromeVox();
  host_helper.WaitForHostCompletedFirstLoad();
  EXPECT_TRUE(GetManager()->IsSpokenFeedbackEnabled());

  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();

  // Hints should show up after a few seconds without speech.
  WaitForProperties(
      /*visible=*/true,
      /*icon=*/DictationBubbleIconType::kStandby,
      /*text=*/std::optional<std::u16string>(),
      /*hints=*/std::vector<std::u16string>{kTrySaying, kType, kHelp});

  // Assert speech from ChromeVox.
  sm.ExpectSpeechPattern("Try saying*Type*Help*");
  sm.Replay();

  // Check that Chromevox changed to a different pitch to announce hints. Note
  // that only if the whole text pattern used the same parameters this will
  // match.
  auto params =
      sm.GetParamsForPreviouslySpokenTextPattern("*Try saying*Type*Help*");
  ASSERT_TRUE(params);

  // Note: the Chromevox personality that sets this value has a relative pitch
  // of 0.3, so that's how this turns into 1.3.
  EXPECT_DOUBLE_EQ(params->pitch, 1.3);
}

IN_PROC_BROWSER_TEST_P(DictationUITest, HintsShownWhenTextCommitted) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();

  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/std::optional<std::u16string>(),
                    /*hints=*/std::optional<std::vector<std::u16string>>());

  // Send a final speech result. UI should return to standby mode.
  SendFinalResultAndWait("Testing");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/std::u16string(),
                    /*hints=*/std::optional<std::vector<std::u16string>>());

  // Hints should show up after a few seconds without speech.
  WaitForProperties(
      /*visible=*/true,
      /*icon=*/DictationBubbleIconType::kStandby,
      /*text=*/std::optional<std::u16string>(),
      /*hints=*/
      std::vector<std::u16string>{kTrySaying, kUndo, kDelete, kSelectAll,
                                  kHelp});
}

IN_PROC_BROWSER_TEST_P(DictationUITest, HintsShownAfterTextSelected) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();

  // Perform a select all command.
  SendFinalResultAndWaitForEditableValue("Vega is the brightest star in Lyra",
                                         "Vega is the brightest star in Lyra");
  SendFinalResultAndWaitForSelection("Select all", 0, 34);
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kMacroSuccess,
                    /*text=*/u"Select all",
                    /*hints=*/std::optional<std::vector<std::u16string>>());

  // UI should return to standby mode with hints after a few seconds without
  // speech.
  WaitForProperties(
      /*visible=*/true,
      /*icon=*/DictationBubbleIconType::kStandby,
      /*text=*/std::optional<std::u16string>(),
      /*hints=*/
      std::vector<std::u16string>{kTrySaying, kUnselect, kCopy, kDelete,
                                  kHelp});
}

IN_PROC_BROWSER_TEST_P(DictationUITest, HintsShownAfterCommandExecuted) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();

  // Perform a command.
  SendFinalResultAndWait("Move to the previous character");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kMacroSuccess,
                    /*text=*/u"Move to the previous character",
                    /*hints=*/std::optional<std::vector<std::u16string>>());

  // UI should return to standby mode with hints after a few seconds without
  // speech.
  WaitForProperties(
      /*visible=*/true,
      /*icon=*/DictationBubbleIconType::kStandby,
      /*text=*/std::optional<std::u16string>(),
      /*hints=*/std::vector<std::u16string>{kTrySaying, kUndo, kHelp});
}

// Tests behavior of Dictation using the Pumpkin semantic parser.
class DictationPumpkinTest : public DictationTest {
 public:
  DictationPumpkinTest() = default;
  ~DictationPumpkinTest() = default;
  DictationPumpkinTest(const DictationPumpkinTest&) = delete;
  DictationPumpkinTest& operator=(const DictationPumpkinTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    DictationTest::SetUpOnMainThread();
    ToggleDictationWithKeystroke();
    WaitForRecognitionStarted();
  }

  void TearDownOnMainThread() override {
    ToggleDictationWithKeystroke();
    WaitForRecognitionStopped();
    DictationTest::TearDownOnMainThread();
  }
};

INSTANTIATE_TEST_SUITE_P(
    NetworkTextArea,
    DictationPumpkinTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kTextArea)));

INSTANTIATE_TEST_SUITE_P(
    NetworkContentEditable,
    DictationPumpkinTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kContentEditable)));

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, Input) {
  SendFinalResultAndWaitForEditableValue("dictate hello", "Hello");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, DeletePrevCharacter) {
  SendFinalResultAndWaitForEditableValue("Testing", "Testing");
  SendFinalResultAndWaitForEditableValue("Delete three characters", "Test");
  SendFinalResultAndWaitForEditableValue("backspace", "Tes");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, NavByCharacter) {
  SendFinalResultAndWaitForEditableValue("Testing", "Testing");
  SendFinalResultAndWaitForSelection("left three characters", 4, 4);
  SendFinalResultAndWaitForEditableValue("!", "Test!ing");
  SendFinalResultAndWaitForSelection("right two characters", 7, 7);
  SendFinalResultAndWaitForEditableValue("@", "Test!in@g");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, NavByLine) {
  if (!RunOnMultilineContent())
    return;

  std::string text = "Line1\nLine2\nLine3\nLine4";
  SendFinalResultAndWaitForEditableValue(text, text);
  SendFinalResultAndWaitForSelection("Up two lines", 11, 11);
  std::string expected = "Line1\nLine2 insertion\nLine3\nLine4";
  SendFinalResultAndWaitForEditableValue("insertion", expected);
  SendFinalResultAndWaitForSelection("down two lines", 33, 33);
  expected = "Line1\nLine2 insertion\nLine3\nLine4 second insertion";
  SendFinalResultAndWaitForEditableValue("second insertion", expected);
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, CutCopyPasteSelectAll) {
  SendFinalResultAndWaitForEditableValue("Star", "Star");
  SendFinalResultAndWaitForSelection("Select everything", 0, 4);
  SendFinalResultAndWaitForClipboardChanged("Copy selected text");
  EXPECT_EQ("Star", GetClipboardText());
  SendFinalResultAndWaitForSelection("unselect selection", 4, 4);
  SendFinalResultAndWaitForEditableValue("paste copied text", "StarStar");
  SendFinalResultAndWaitForSelection("highlight everything", 0, 8);
  SendFinalResultAndWaitForClipboardChanged("cut highlighted text");
  EXPECT_EQ("StarStar", GetClipboardText());
  WaitForEditableValue(
      (editable_type() == EditableType::kContentEditable) ? "\n" : "");
  SendFinalResultAndWaitForEditableValue("paste the copied text", "StarStar");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, UndoAndRedo) {
  SendFinalResultAndWaitForEditableValue("The constellation",
                                         "The constellation");
  SendFinalResultAndWaitForEditableValue("myra", "The constellation myra");
  SendFinalResultAndWaitForEditableValue("take that back", "The constellation");
  SendFinalResultAndWaitForEditableValue("Lyra", "The constellation lyra");
  SendFinalResultAndWaitForEditableValue("undo that", "The constellation");
  SendFinalResultAndWaitForEditableValue("redo that", "The constellation lyra");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, DeletePrevWord) {
  SendFinalResultAndWaitForEditableValue("This is a test", "This is a test");
  SendFinalResultAndWaitForEditableValue("clear one word", "This is a ");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, DeletePrevSent) {
  SendFinalResultAndWaitForEditableValue("Hello, world.", "Hello, world.");
  SendFinalResultAndWaitForEditableValue(
      "erase sentence",
      (editable_type() == EditableType::kContentEditable) ? "\n" : "");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, MoveByWord) {
  SendFinalResultAndWaitForEditableValue("This is a quiz", "This is a quiz");
  SendFinalResultAndWaitForSelection("back one word", 10, 10);
  SendFinalResultAndWaitForEditableValue("pop", "This is a pop quiz");
  SendFinalResultAndWaitForSelection("right one word", 18, 18);
  SendFinalResultAndWaitForEditableValue("folks!", "This is a pop quiz folks!");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, SmartDeletePhrase) {
  SendFinalResultAndWaitForEditableValue("This is a difficult test",
                                         "This is a difficult test");
  SendFinalResultAndWaitForEditableValue("erase difficult", "This is a test");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, SmartReplacePhrase) {
  SendFinalResultAndWaitForEditableValue("This is a difficult test.",
                                         "This is a difficult test.");
  SendFinalResultAndWaitForEditableValue("substitute difficult with simple",
                                         "This is a simple test.");
  SendFinalResultAndWaitForEditableValue("replace is with isn't",
                                         "This isn't a simple test.");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, SmartInsertBefore) {
  SendFinalResultAndWaitForEditableValue("This is a test.", "This is a test.");
  SendFinalResultAndWaitForEditableValue("insert simple in front of test",
                                         "This is a simple test.");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, SmartSelectBetween) {
  SendFinalResultAndWaitForEditableValue("This is a test.", "This is a test.");
  SendFinalResultAndWaitForSelection("highlight everything between is and test",
                                     5, 14);
  SendFinalResultAndWaitForEditableValue("was a quiz", "This was a quiz.");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, MoveBySentence) {
  SendFinalResultAndWaitForEditableValue("Hello world! Goodnight world?",
                                         "Hello world! Goodnight world?");
  SendFinalResultAndWaitForSelection("one sentence back", 12, 12);
  SendFinalResultAndWaitForEditableValue(
      "Good evening.", "Hello world! Good evening. Goodnight world?");
  SendFinalResultAndWaitForSelection("forward one sentence", 43, 43);
  SendFinalResultAndWaitForEditableValue(
      "Time for a midnight snack",
      "Hello world! Good evening. Goodnight world? Time for a midnight snack");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, DeleteAllText) {
  SendFinalResultAndWaitForEditableValue("Hello, world.", "Hello, world.");
  SendFinalResultAndWaitForEditableValue(
      "clear", (editable_type() == EditableType::kContentEditable) ? "\n" : "");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, NavStartText) {
  SendFinalResultAndWaitForEditableValue("Is good", "Is good");
  SendFinalResultAndWaitForSelection("to start", 0, 0);
  SendFinalResultAndWaitForEditableValue("The weather outside",
                                         "The weather outside Is good");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, NavEndText) {
  SendFinalResultAndWaitForEditableValue("The weather outside is",
                                         "The weather outside is");
  SendFinalResultAndWaitForSelection("to start", 0, 0);
  SendFinalResultAndWaitForSelection("to end", 22, 22);
  std::string expected = "The weather outside is good";
  SendFinalResultAndWaitForEditableValue("good", expected);
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, SelectPrevWord) {
  SendFinalResultAndWaitForEditableValue("The weather today is bad",
                                         "The weather today is bad");
  SendFinalResultAndWaitForSelection("highlight back one word", 21, 24);
  std::string expected = "The weather today is nice";
  SendFinalResultAndWaitForEditableValue("nice", expected);
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, SelectNextWord) {
  SendFinalResultAndWaitForEditableValue("The weather today is bad",
                                         "The weather today is bad");
  SendFinalResultAndWaitForSelection("move to the previous word", 21, 21);
  SendFinalResultAndWaitForSelection("highlight right one word", 21, 24);
  std::string expected = "The weather today is nice";
  SendFinalResultAndWaitForEditableValue("nice", expected);
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, SelectNextChar) {
  SendFinalResultAndWaitForEditableValue("Text", "Text");
  SendFinalResultAndWaitForSelection("move to the previous word", 0, 0);
  SendFinalResultAndWaitForSelection("select next letter", 0, 1);
  std::string expected = "ext";
  SendFinalResultAndWaitForEditableValue("delete", expected);
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, SelectPrevChar) {
  SendFinalResultAndWaitForEditableValue("Text", "Text");
  SendFinalResultAndWaitForSelection("select previous letter", 3, 4);
  std::string expected = "Tex";
  SendFinalResultAndWaitForEditableValue("delete", expected);
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, Repeat) {
  SendFinalResultAndWaitForEditableValue("Test", "Test");
  SendFinalResultAndWaitForEditableValue("delete", "Tes");
  SendFinalResultAndWaitForEditableValue("try that action again", "Te");
  // Repeat also works for inputting text.
  SendFinalResultAndWaitForEditableValue("keyboard cat", "Te keyboard cat");
  SendFinalResultAndWaitForEditableValue("again",
                                         "Te keyboard cat keyboard cat");
}

IN_PROC_BROWSER_TEST_P(DictationPumpkinTest, Metrics) {
  base::HistogramTester histogram_tester_;
  HistogramWaiter used_waiter(kPumpkinUsedMetric);
  HistogramWaiter succeeded_waiter(kPumpkinSucceededMetric);
  SendFinalResultAndWaitForEditableValue("dictate hello", "Hello");
  used_waiter.Wait();
  succeeded_waiter.Wait();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester_.ExpectUniqueSample(/*name=*/kPumpkinUsedMetric,
                                       /*sample=*/true,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(/*name=*/kPumpkinSucceededMetric,
                                       /*sample=*/true,
                                       /*expected_bucket_count=*/1);
}

class DictationContextCheckingTest : public DictationTest {
 public:
  DictationContextCheckingTest() = default;
  ~DictationContextCheckingTest() override = default;
  DictationContextCheckingTest(const DictationContextCheckingTest&) = delete;
  DictationContextCheckingTest& operator=(const DictationContextCheckingTest&) =
      delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DictationTest::SetUpCommandLine(command_line);

    std::vector<base::test::FeatureRef> enabled_features;
    enabled_features.emplace_back(base::test::FeatureRef(
        ::features::kExperimentalAccessibilityDictationContextChecking));
    scoped_feature_list_.InitWithFeatures(
        enabled_features, std::vector<base::test::FeatureRef>());
  }

  void SetUpOnMainThread() override {
    DictationTest::SetUpOnMainThread();
    ToggleDictationWithKeystroke();
    WaitForRecognitionStarted();
    dictation_bubble_test_helper_ =
        std::make_unique<DictationBubbleTestHelper>();
  }

  void TearDownOnMainThread() override {
    ToggleDictationWithKeystroke();
    WaitForRecognitionStopped();
    DictationTest::TearDownOnMainThread();
  }

  void WaitForProperties(
      bool visible,
      DictationBubbleIconType icon,
      const std::optional<std::u16string>& text,
      const std::optional<std::vector<std::u16string>>& hints) {
    dictation_bubble_test_helper_->WaitForVisibility(visible);
    dictation_bubble_test_helper_->WaitForVisibleIcon(icon);
    if (text.has_value()) {
      dictation_bubble_test_helper_->WaitForVisibleText(text.value());
    }
    if (hints.has_value()) {
      dictation_bubble_test_helper_->WaitForVisibleHints(hints.value());
    }
  }

  // Attempts to run `command` within an empty editable and waits for the UI to
  // show the appropriate context-checking failure.
  void RunEmptyEditableTest(const std::string& command) {
    SendFinalResultAndWait(command);
    std::u16string message =
        u"Can't " + base::ASCIIToUTF16(command) + u", text field is empty";
    WaitForProperties(
        /*visible=*/true,
        /*icon=*/DictationBubbleIconType::kMacroFail,
        /*text=*/message,
        /*hints=*/std::optional<std::vector<std::u16string>>());
  }

  // Attempts to run `command` on an editable with no selection and waits for
  // the UI to show the appropriate context-checking failure.
  void RunNoSelectionTest(const std::string& command) {
    SendFinalResultAndWaitForEditableValue("Hello world", "Hello world");
    SendFinalResultAndWait(command);
    std::u16string message =
        u"Can't " + base::ASCIIToUTF16(command) + u", no selected text";
    WaitForProperties(
        /*visible=*/true,
        /*icon=*/DictationBubbleIconType::kMacroFail,
        /*text=*/message,
        /*hints=*/std::optional<std::vector<std::u16string>>());
    SendFinalResultAndWaitForEditableValue(
        "delete all",
        (editable_type() == EditableType::kContentEditable) ? "\n" : "");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<DictationBubbleTestHelper> dictation_bubble_test_helper_;
};

INSTANTIATE_TEST_SUITE_P(
    NetworkInput,
    DictationContextCheckingTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kInput)));

INSTANTIATE_TEST_SUITE_P(
    NetworkContentEditable,
    DictationContextCheckingTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kContentEditable)));

IN_PROC_BROWSER_TEST_P(DictationContextCheckingTest, EmptyEditable) {
  std::vector<std::string> commands{
      "unselect",
      "cut",
      "copy",
      "delete one character",
      "left one character",
      "right one character",
      "up one line",
      "down one line",
      "select all",
      "delete one word",
      "delete one sentence",
      "right one word",
      "left one word",
      "delete the phrase hello",
      "replace hello with world",
      "insert hello before world",
      "select between hello and world",
      "left one sentence",
      "right one sentence",
      "delete all",
      "to start",
      "to end",
      "select previous word",
      "select next word",
      "select previous letter",
      "select next letter",
  };

  for (const auto& command : commands) {
    RunEmptyEditableTest(command);
  }
}

IN_PROC_BROWSER_TEST_P(DictationContextCheckingTest, NoSelection) {
  std::vector<std::string> commands{
      "unselect",
      "cut",
      "copy",
  };

  for (const auto& command : commands) {
    RunNoSelectionTest(command);
  }
}

IN_PROC_BROWSER_TEST_P(DictationContextCheckingTest, UnselectSuccessful) {
  std::string text = "Hello world";
  SendFinalResultAndWaitForEditableValue(text, text);
  SendFinalResultAndWaitForSelection("Select all", 0, 11);
  SendFinalResultAndWaitForSelection("Unselect", 11, 11);
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kMacroSuccess,
                    /*text=*/std::optional<std::u16string>(),
                    /*hints=*/std::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationContextCheckingTest, CutSuccessful) {
  std::string text = "Hello world";
  SendFinalResultAndWaitForEditableValue(text, text);
  SendFinalResultAndWaitForSelection("Select all", 0, 11);
  SendFinalResultAndWaitForClipboardChanged("Cut");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kMacroSuccess,
                    /*text=*/std::optional<std::u16string>(),
                    /*hints=*/std::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationContextCheckingTest, CopySuccessful) {
  std::string text = "Hello world";
  SendFinalResultAndWaitForEditableValue(text, text);
  SendFinalResultAndWaitForSelection("Select all", 0, 11);
  SendFinalResultAndWaitForClipboardChanged("Copy");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kMacroSuccess,
                    /*text=*/std::optional<std::u16string>(),
                    /*hints=*/std::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationContextCheckingTest, RepeatFail) {
  SendFinalResultAndWait("repeat");
  WaitForProperties(
      /*visible=*/true,
      /*icon=*/DictationBubbleIconType::kMacroFail,
      /*text=*/u"Can't repeat, no previous command",
      /*hints=*/std::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationContextCheckingTest, RepeatFailUnselect) {
  RunEmptyEditableTest("unselect");
  // Wait for UI to return to standby mode.
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/std::optional<std::u16string>(),
                    /*hints=*/std::optional<std::vector<std::u16string>>());
  RunEmptyEditableTest("repeat");
}

IN_PROC_BROWSER_TEST_P(DictationContextCheckingTest, RepeatSuccessful) {
  SendFinalResultAndWaitForEditableValue("Test", "Test");
  SendFinalResultAndWaitForEditableValue("Repeat", "Test test");
  SendFinalResultAndWaitForEditableValue("Repeat", "Test test test");
}

class NotificationCenterDictationTest : public DictationTest {
 public:
  NotificationCenterDictationTest() = default;
  NotificationCenterDictationTest(const NotificationCenterDictationTest&) =
      delete;
  NotificationCenterDictationTest& operator=(
      const NotificationCenterDictationTest&) = delete;
  ~NotificationCenterDictationTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DictationTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    DictationTest::SetUpOnMainThread();
    ToggleDictationWithKeystroke();
    WaitForRecognitionStarted();
  }

  void TearDownOnMainThread() override {
    ToggleDictationWithKeystroke();
    WaitForRecognitionStopped();
    DictationTest::TearDownOnMainThread();
  }

  NotificationCenterTestApi* test_api() {
    if (!test_api_) {
      test_api_ = std::make_unique<NotificationCenterTestApi>();
    }
    return test_api_.get();
  }

 private:
  std::unique_ptr<NotificationCenterTestApi> test_api_;
};

INSTANTIATE_TEST_SUITE_P(
    NetworkTextArea,
    NotificationCenterDictationTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kTextArea)));

// Tests that clicking the notification center tray does not crash when
// dictation is enabled.
IN_PROC_BROWSER_TEST_P(NotificationCenterDictationTest, OpenBubble) {
  // Add a notification to ensure the tray is visible.
  test_api()->AddNotification();
  ASSERT_TRUE(test_api()->IsTrayShown());

  // Click on the tray and verify the bubble shows up.
  test_api()->ToggleBubble();
  EXPECT_TRUE(test_api()->GetWidget()->IsActive());
  EXPECT_TRUE(test_api()->IsBubbleShown());
}

// A test class that only runs on formatted content editables.
class DictationFormattedContentEditableTest : public DictationPumpkinTest {
 public:
  DictationFormattedContentEditableTest() = default;
  ~DictationFormattedContentEditableTest() override = default;
  DictationFormattedContentEditableTest(
      const DictationFormattedContentEditableTest&) = delete;
  DictationFormattedContentEditableTest& operator=(
      const DictationFormattedContentEditableTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    DictationPumpkinTest::SetUpOnMainThread();

    // Place the selection at the end of the content editable (the pre-populated
    // editable value has a length of 14).
    std::string script = "dictationTestSupport.setSelection(14, 14);";
    ExecuteAccessibilityCommonScript(script);
  }
};

// Note: For these tests, the content editable comes pre-populated with a value
// of "This is a test". See `kFormattedContentEditableUrl` for more details.
INSTANTIATE_TEST_SUITE_P(
    NetworkFormattedContentEditable,
    DictationFormattedContentEditableTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kFormattedContentEditable)));

IN_PROC_BROWSER_TEST_P(DictationFormattedContentEditableTest, DeletePhrase) {
  SendFinalResultAndWaitForEditableValue("delete a", "This is test");
}

IN_PROC_BROWSER_TEST_P(DictationFormattedContentEditableTest,
                       ReplacePhraseMultipleWords) {
  std::string command = "replace the phrase is a test with was just one exam";
  std::string expected_value = "This was just one exam";
  SendFinalResultAndWaitForEditableValue(command, expected_value);
}

IN_PROC_BROWSER_TEST_P(DictationFormattedContentEditableTest,
                       ReplacePhraseFirstWord) {
  std::string command = "replace this with it";
  std::string expected_value = "It is a test";
  SendFinalResultAndWaitForEditableValue(command, expected_value);
}

IN_PROC_BROWSER_TEST_P(DictationFormattedContentEditableTest,
                       ReplacePhraseLastWord) {
  std::string command = "replace test with quiz";
  std::string expected_value = "This is a quiz";
  SendFinalResultAndWaitForEditableValue(command, expected_value);
}

IN_PROC_BROWSER_TEST_P(DictationFormattedContentEditableTest, InsertBefore) {
  std::string command = "insert the phrase simple before test";
  std::string expected_value = "This is a simple test";
  SendFinalResultAndWaitForEditableValue(command, expected_value);
}

IN_PROC_BROWSER_TEST_P(DictationFormattedContentEditableTest, MoveBySentence) {
  SendFinalResultAndWaitForEditableValue(", good luck.",
                                         "This is a test, good luck.");
  SendFinalResultAndWaitForSelection("move to the previous sentence", 0, 0);
  SendFinalResultAndWaitForEditableValue(
      "Good morning. ", "Good morning. This is a test, good luck.");
  SendFinalResultAndWaitForSelection("forward one sentence", 40, 40);
  SendFinalResultAndWaitForEditableValue(
      " Have fun.", "Good morning. This is a test, good luck. Have fun.");
}

IN_PROC_BROWSER_TEST_P(DictationFormattedContentEditableTest,
                       SmartSelectBetween) {
  SendFinalResultAndWaitForSelection("highlight everything between is and a", 5,
                                     9);
  SendFinalResultAndWaitForEditableValue("was one", "This was one test");
}

class AccessibilityToastCallbackManager {
 public:
  void OnToastShown(AccessibilityToastType type) {
    if (type == type_) {
      run_loop_.Quit();
    }
  }

  void WaitForToastShown(AccessibilityToastType type) {
    type_ = type;
    run_loop_.Run();
  }

 private:
  AccessibilityToastType type_;
  base::RunLoop run_loop_;
};

class AudioCallbackManager {
 public:
  void OnSetMute(bool success) {
    if (success) {
      run_loop_.Quit();
    }
  }

  void WaitForSetMute() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

class DictationKeyboardImprovementsTest : public DictationTestBase {
 public:
  DictationKeyboardImprovementsTest() = default;
  ~DictationKeyboardImprovementsTest() override = default;
  DictationKeyboardImprovementsTest(const DictationKeyboardImprovementsTest&) =
      delete;
  DictationKeyboardImprovementsTest& operator=(
      const DictationKeyboardImprovementsTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    DictationTestBase::SetUpOnMainThread();
    test_api_ = AccessibilityControllerTestApi::Create();
    toast_callback_manager_ =
        std::make_unique<AccessibilityToastCallbackManager>();
    test_api_->AddShowToastCallbackForTesting(
        base::BindRepeating(&AccessibilityToastCallbackManager::OnToastShown,
                            base::Unretained(toast_callback_manager_.get())));
  }

  void TabAwayFromEditableAndReduceNoFocusedImeTimeout() {
    PressTab();
    // Reduce the no focused IME timeout so that the nudge will be shown
    // promptly.
    ExecuteAccessibilityCommonScript(
        "dictationTestSupport.setNoFocusedImeTimeout(500);");
  }

  void WaitForToastShown(AccessibilityToastType type) {
    toast_callback_manager_->WaitForToastShown(type);
  }

  void MuteMicrophone(bool mute) {
    AudioCallbackManager audio_callback_manager = AudioCallbackManager();
    extensions::AudioService* service =
        extensions::AudioAPI::GetFactoryInstance()
            ->Get(GetProfile())
            ->GetService();
    service->SetMute(/*is_input=*/true, /*is_muted=*/mute,
                     base::BindOnce(&AudioCallbackManager::OnSetMute,
                                    base::Unretained(&audio_callback_manager)));
    audio_callback_manager.WaitForSetMute();
  }

 private:
  std::unique_ptr<AccessibilityControllerTestApi> test_api_;
  std::unique_ptr<AccessibilityToastCallbackManager> toast_callback_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    NetworkDictationKeyboardImprovementsTest,
    DictationKeyboardImprovementsTest,
    ::testing::Values(TestConfig(speech::SpeechRecognitionType::kNetwork,
                                 EditableType::kInput)));

// Verifies that a nudge is shown in the system UI when Dictation is toggled
// when there is no focused editable.
IN_PROC_BROWSER_TEST_P(DictationKeyboardImprovementsTest,
                       ToggledWithNoFocusShowsNudge) {
  TabAwayFromEditableAndReduceNoFocusedImeTimeout();
  // Disable the console observer because toggling Dictation in the following
  // manner will cause an error to be emitted to the console.
  utils()->DisableConsoleObserver();
  ToggleDictationWithKeystroke();
  WaitForToastShown(AccessibilityToastType::kDictationNoFocusedTextField);
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationKeyboardImprovementsTest,
                       ToggledWithMuteShowsNudge) {
  MuteMicrophone(true);
  // Disable the console observer because toggling Dictation in the following
  // manner will cause an error to be emitted to the console.
  utils()->DisableConsoleObserver();
  ToggleDictationWithKeystroke();
  WaitForToastShown(AccessibilityToastType::kDictationMicMuted);
  WaitForRecognitionStopped();

  MuteMicrophone(false);
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForEditableValue("Testing", "Testing");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

// Verifies that ChromeVox announces a message when when Dictation is toggled
// when there is no focused editable.
// TODO(b:259352600): Flaky on MSAN & ASAN and Linux ChromiumOS in general.
IN_PROC_BROWSER_TEST_P(DictationKeyboardImprovementsTest,
                       DISABLED_ToggledWithNoFocusTriggersSpeech) {
  TabAwayFromEditableAndReduceNoFocusedImeTimeout();
  // Setup ChromeVox.
  test::SpeechMonitor sm;
  EXPECT_FALSE(GetManager()->IsSpokenFeedbackEnabled());
  extensions::ExtensionHostTestHelper host_helper(
      GetProfile(), extension_misc::kChromeVoxExtensionId);
  EnableChromeVox();
  host_helper.WaitForHostCompletedFirstLoad();
  EXPECT_TRUE(GetManager()->IsSpokenFeedbackEnabled());

  // Disable the console observer because toggling Dictation in the following
  // manner will cause an error to be emitted to the console.
  utils()->DisableConsoleObserver();
  ToggleDictationWithKeystroke();
  WaitForToastShown(AccessibilityToastType::kDictationNoFocusedTextField);
  WaitForRecognitionStopped();

  // Assert speech from ChromeVox.
  sm.ExpectSpeechPattern("*Go to a text field to use Dictation*");
  sm.Replay();
}

}  // namespace ash
