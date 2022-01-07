// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/hash/hash.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/input_method/textinput_test_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "chrome/browser/speech/speech_recognition_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/mock_ime_input_context_handler.h"
#include "ui/base/ime/fake_text_input_client.h"
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
const char* kOnDeviceListeningDurationMetric =
    "Accessibility.CrosDictation.ListeningDuration.OnDeviceRecognition";
const char* kNetworkListeningDurationMetric =
    "Accessibility.CrosDictation.ListeningDuration.NetworkRecognition";
const char* kLocaleMetric = "Accessibility.CrosDictation.Language";
const char* kOnDeviceSpeechMetric =
    "Accessibility.CrosDictation.UsedOnDeviceSpeech";

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
    "new line"};

PrefService* GetActiveUserPrefs() {
  return ProfileManager::GetActiveUserProfile()->GetPrefs();
}

AccessibilityManager* GetManager() {
  return AccessibilityManager::Get();
}

void EnableChromeVox() {
  GetManager()->EnableSpokenFeedback(true);
}

// Listens for changes to the histogram provided at construction. This class
// only allows `Wait()` to be called once. If you need to call `Wait()` multiple
// times, create multiple instances of this class.
class HistogramWaiterOneShot {
 public:
  explicit HistogramWaiterOneShot(const char* metric_name) {
    histogram_observer_ = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        metric_name,
        base::BindRepeating(&HistogramWaiterOneShot::OnHistogramCallback,
                            base::Unretained(this)));
  }
  ~HistogramWaiterOneShot() { histogram_observer_.reset(); }

  HistogramWaiterOneShot(const HistogramWaiterOneShot&) = delete;
  HistogramWaiterOneShot& operator=(const HistogramWaiterOneShot&) = delete;

  // Waits for the next update to the observed histogram.
  void Wait() { run_loop_.Run(); }

  void OnHistogramCallback(const char* metric_name,
                           uint64_t name_hash,
                           base::HistogramBase::Sample sample) {
    run_loop_.Quit();
    histogram_observer_.reset();
  }

 private:
  std::unique_ptr<base::StatisticsRecorder::ScopedHistogramSampleObserver>
      histogram_observer_;
  base::RunLoop run_loop_;
};

// A class that repeatedly runs a function, which is supplied at construction,
// until it evaluates to true.
class SuccessWaiter {
 public:
  explicit SuccessWaiter(base::RepeatingCallback<bool()> is_success)
      : is_success_(std::move(is_success)) {}
  ~SuccessWaiter() = default;
  SuccessWaiter(const SuccessWaiter&) = delete;
  SuccessWaiter& operator=(const SuccessWaiter&) = delete;

  void Wait() {
    base::RepeatingTimer timer;
    timer.Start(FROM_HERE, base::Milliseconds(10), this,
                &SuccessWaiter::OnTimer);
    run_loop_.Run();
  }

  void OnTimer() {
    if (is_success_.Run())
      run_loop_.Quit();
  }

 private:
  base::RepeatingCallback<bool()> is_success_;
  base::RunLoop run_loop_;
};

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
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {
    run_loop_.Quit();
  }

  ui::InputMethod* input_method_;
  base::RunLoop run_loop_;
};

// Listens for changes to the clipboard. This class only allows `Wait()` to be
// called once. If you need to call `Wait()` multiple times, create multiple
// instances of this class.
class ClipboardChangedWaiterOneShot : public ui::ClipboardObserver {
 public:
  ClipboardChangedWaiterOneShot() {
    ui::ClipboardMonitor::GetInstance()->AddObserver(this);
  }
  ClipboardChangedWaiterOneShot(const ClipboardChangedWaiterOneShot&) = delete;
  ClipboardChangedWaiterOneShot& operator=(
      const ClipboardChangedWaiterOneShot&) = delete;
  ~ClipboardChangedWaiterOneShot() override {
    ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

 private:
  // ui::ClipboardObserver:
  void OnClipboardDataChanged() override { run_loop_.Quit(); }

  base::RunLoop run_loop_;
};

}  // namespace

// This class performs common setup and teardown operations for Dictation tests,
// such as managing the speech recognition service.
class DictationBaseTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<speech::SpeechRecognitionType> {
 protected:
  DictationBaseTest() : test_helper_(GetParam()) {}
  ~DictationBaseTest() override = default;
  DictationBaseTest(const DictationBaseTest&) = delete;
  DictationBaseTest& operator=(const DictationBaseTest&) = delete;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::Feature> enabled_features =
        test_helper_.GetEnabledFeatures();
    std::vector<base::Feature> disabled_features =
        test_helper_.GetDisabledFeatures();
    if (GetParam() == speech::SpeechRecognitionType::kOnDevice) {
      enabled_features.push_back(
          ::features::kExperimentalAccessibilityDictationOffline);
    } else {
      disabled_features.push_back(
          ::features::kExperimentalAccessibilityDictationOffline);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    test_helper_.SetUp(browser()->profile());
    // Ensure that the Dictation locale preference is set.
    GetActiveUserPrefs()->SetString(prefs::kAccessibilityDictationLocale,
                                    "en-US");
  }

  void TearDownOnMainThread() override {
    if (GetParam() == speech::SpeechRecognitionType::kNetwork)
      content::SpeechRecognitionManager::SetManagerForTesting(nullptr);

    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Routers to SpeechRecognitionTestHelper methods.
  void WaitForRecognitionStarted() { test_helper_.WaitForRecognitionStarted(); }

  void WaitForRecognitionStopped() { test_helper_.WaitForRecognitionStopped(); }

  void SendFakeSpeechResultAndWait(const std::string& transcript,
                                   bool is_final) {
    test_helper_.SendFakeSpeechResultAndWait(transcript, is_final);
  }

  void SendFinalFakeSpeechResultAndWait(const std::string& transcript) {
    test_helper_.SendFinalFakeSpeechResultAndWait(transcript);
  }

  void SendFakeSpeechRecognitionErrorAndWait() {
    test_helper_.SendFakeSpeechRecognitionErrorAndWait();
  }

 private:
  SpeechRecognitionTestHelper test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DictationTest : public DictationBaseTest {
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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DictationBaseTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitAndDisableFeature(
        ::features::kExperimentalAccessibilityDictationExtension);
  }

  void SetUpOnMainThread() override {
    DictationBaseTest::SetUpOnMainThread();

    ui::IMEBridge::Get()->SetInputContextHandler(input_context_handler_.get());
    generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::Get()->GetPrimaryRootWindow());
    GetActiveUserPrefs()->SetBoolean(
        prefs::kDictationAcceleratorDialogHasBeenAccepted, true);
    ash::Shell::Get()->accessibility_controller()->dictation().SetEnabled(true);
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
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    Network,
    DictationTest,
    ::testing::Values(speech::SpeechRecognitionType::kNetwork));

INSTANTIATE_TEST_SUITE_P(
    OnDevice,
    DictationTest,
    ::testing::Values(speech::SpeechRecognitionType::kOnDevice));

IN_PROC_BROWSER_TEST_P(DictationTest, RecognitionEnds) {
  ToggleDictation();
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendFakeSpeechResultAndWait(kFirstSpeechResult, false /* is_final */);
  EXPECT_EQ(kFirstSpeechResult16, GetLastCompositionText().text);

  SendFakeSpeechResultAndWait(kSecondSpeechResult, false /* is_final */);
  EXPECT_EQ(kSecondSpeechResult16, GetLastCompositionText().text);

  SendFakeSpeechResultAndWait(kFinalSpeechResult, true /* is_final */);
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

  SendFakeSpeechResultAndWait(kFirstSpeechResult, false /* is_final */);
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendFakeSpeechResultAndWait(kSecondSpeechResult, false /* is_final */);
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendFakeSpeechResultAndWait(kFinalSpeechResult, true /* is_final */);
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
  SendFakeSpeechResultAndWait(kFirstSpeechResult, false /* is_final */);
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

  SendFakeSpeechResultAndWait(kFinalSpeechResult, false /* is_final */);
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

  SendFakeSpeechResultAndWait(kFinalSpeechResult, false /* is_final */);
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
  SendFakeSpeechResultAndWait(kFirstSpeechResult, true /* is final */);
  // Wait for interim results to be finalized.
  base::RunLoop().RunUntilIdle();

  // Speech goes to the default IMEInputContextHandler.
  EXPECT_EQ(kFirstSpeechResult16, input_context_handler_->last_commit_text());

  // Simulate a remote app instantiating a new IMEInputContextHandler, like
  // the keyboard shortcut viewer app creating a second `InputMethodAsh`.
  ui::MockIMEInputContextHandler input_context_handler2;
  ui::IMEBridge::Get()->SetInputContextHandler(&input_context_handler2);

  SendFakeSpeechResultAndWait(kSecondSpeechResult, true /* is final*/);
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
  SendFakeSpeechResultAndWait(kFinalSpeechResult, false /* is_final */);

  // Change the input state to a new client.
  std::unique_ptr<ui::TextInputClient> new_client =
      std::make_unique<ui::FakeTextInputClient>(ui::TEXT_INPUT_TYPE_TEXT);
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
  SendFakeSpeechResultAndWait("Purple", true /* is final */);
  // Wait for interim results to be finalized.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(u"Purple", input_context_handler_->last_commit_text());
  EXPECT_FALSE(IsDictationOff());

  SendFakeSpeechResultAndWait("pink", true /* is final */);
  EXPECT_EQ(2, input_context_handler_->commit_text_call_count());
  // Space in front of the result.
  EXPECT_EQ(u" pink", input_context_handler_->last_commit_text());

  SendFakeSpeechResultAndWait(" blue", true /* is final */);
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
  auto locales = GetAllSupportedLocales();
  for (auto& it : locales) {
    const std::string locale = it.first;
    bool works_offline = it.second.works_offline;
    bool installed = it.second.installed;
    if (GetParam() == speech::SpeechRecognitionType::kOnDevice &&
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

  if (GetParam() == speech::SpeechRecognitionType::kOnDevice) {
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

// Ensures that the correct metrics are recorded when Dictation is toggled.
IN_PROC_BROWSER_TEST_P(DictationTest, Metrics) {
  base::HistogramTester histogram_tester_;
  bool on_device = GetParam() == speech::SpeechRecognitionType::kOnDevice;
  const char* metric_name = on_device ? kOnDeviceListeningDurationMetric
                                      : kNetworkListeningDurationMetric;
  HistogramWaiterOneShot waiter(metric_name);
  ToggleDictation();
  WaitForRecognitionStarted();
  ToggleDictation();
  WaitForRecognitionStopped();
  waiter.Wait();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Ensure that we recorded the correct locale.
  const std::string locale = on_device ? "en-US" : "en";
  histogram_tester_.ExpectUniqueSample(/*name=*/kLocaleMetric,
                                       /*sample=*/base::HashMetricName(locale),
                                       /*expected_bucket_count=*/1);
  // Ensure that we recorded the type of speech recognition and listening
  // duration.
  if (on_device) {
    histogram_tester_.ExpectUniqueSample(/*name=*/kOnDeviceSpeechMetric,
                                         /*sample=*/true,
                                         /*expected_bucket_count=*/1);
    ASSERT_EQ(1,
              histogram_tester_.GetAllSamples(kOnDeviceListeningDurationMetric)
                  .size());
    // Ensure there are no metrics for the other type of speech recognition.
    ASSERT_EQ(0,
              histogram_tester_.GetAllSamples(kNetworkListeningDurationMetric)
                  .size());
  } else {
    histogram_tester_.ExpectUniqueSample(/*name=*/kOnDeviceSpeechMetric,
                                         /*sample=*/false,
                                         /*expected_bucket_count=*/1);
    ASSERT_EQ(1,
              histogram_tester_.GetAllSamples(kNetworkListeningDurationMetric)
                  .size());
    // Ensure there are no metrics for the other type of speech recognition.
    ASSERT_EQ(0,
              histogram_tester_.GetAllSamples(kOnDeviceListeningDurationMetric)
                  .size());
  }
}

class DictationExtensionTest : public DictationBaseTest {
 protected:
  DictationExtensionTest() {}
  ~DictationExtensionTest() override = default;
  DictationExtensionTest(const DictationExtensionTest&) = delete;
  DictationExtensionTest& operator=(const DictationExtensionTest&) = delete;

  void SetUpOnMainThread() override {
    DictationBaseTest::SetUpOnMainThread();

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
    DictationBaseTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kExperimentalAccessibilityDictationExtension);
  }

  void SendFinalSpeechResultAndWaitForTextAreaValue(const std::string& result,
                                                    const std::string& value) {
    SendFinalFakeSpeechResultAndWait(result);
    WaitForTextAreaValue(value);
  }

  std::string GetTextAreaValue() {
    std::string output;
    std::string script =
        "window.domAutomationController.send("
        "document.getElementById('textarea').value)";
    CHECK(ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetWebContentsAt(0), script, &output));
    return output;
  }

  void WaitForTextAreaValue(const std::string& value) {
    SuccessWaiter waiter(
        base::BindRepeating(&DictationExtensionTest::TextAreaValueEquals,
                            base::Unretained(this), value));
    waiter.Wait();
    base::RunLoop().RunUntilIdle();
  }

  void ToggleDictationWithKeystroke() {
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, ui::KeyboardCode::VKEY_D, false, false, false, true)));
  }

  void InstallMockInputContextHandler() {
    input_context_handler_ = std::make_unique<ui::MockIMEInputContextHandler>();
    ui::IMEBridge::Get()->SetInputContextHandler(input_context_handler_.get());
  }

  // Retrieves the number of times pre-edit text (composition text) is updated.
  int GetUpdatePreeditTextCallCount() {
    DCHECK(input_context_handler_);
    return input_context_handler_->update_preedit_text_call_count();
  }

  // Retrieves the number of times commit text is updated.
  int GetCommitTextCallCount() {
    DCHECK(input_context_handler_);
    return input_context_handler_->commit_text_call_count();
  }

  void WaitForCompositionText(const std::u16string& value) {
    DCHECK(input_context_handler_);
    SuccessWaiter waiter(
        base::BindRepeating(&DictationExtensionTest::CompositionTextEquals,
                            base::Unretained(this), value));
    waiter.Wait();
    base::RunLoop().RunUntilIdle();
  }

  void WaitForCommitText(const std::u16string& value) {
    DCHECK(input_context_handler_);
    SuccessWaiter waiter(
        base::BindRepeating(&DictationExtensionTest::CommitTextEquals,
                            base::Unretained(this), value));
    waiter.Wait();
    base::RunLoop().RunUntilIdle();
  }

 private:
  bool TextAreaValueEquals(const std::string& value) {
    return value == GetTextAreaValue();
  }

  bool CompositionTextEquals(const std::u16string& value) {
    DCHECK(input_context_handler_);
    return value == input_context_handler_->last_update_composition_arg()
                        .composition_text.text;
  }

  bool CommitTextEquals(const std::u16string& value) {
    DCHECK(input_context_handler_);
    return value == input_context_handler_->last_commit_text();
  }

  std::unique_ptr<ui::MockIMEInputContextHandler> input_context_handler_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    Network,
    DictationExtensionTest,
    ::testing::Values(speech::SpeechRecognitionType::kNetwork));

INSTANTIATE_TEST_SUITE_P(
    OnDevice,
    DictationExtensionTest,
    ::testing::Values(speech::SpeechRecognitionType::kOnDevice));

IN_PROC_BROWSER_TEST_P(DictationExtensionTest, StartsAndStopsRecognition) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationExtensionTest, EntersFinalizedSpeech) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalSpeechResultAndWaitForTextAreaValue(kFinalSpeechResult,
                                               kFinalSpeechResult);
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationExtensionTest, EntersMultipleFinalizedStrings) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalSpeechResultAndWaitForTextAreaValue("The rain in Spain",
                                               "The rain in Spain");
  SendFinalSpeechResultAndWaitForTextAreaValue(
      " falls mainly on the plain.",
      "The rain in Spain falls mainly on the plain.");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationExtensionTest,
                       RecognitionEndsWhenInputFieldLosesFocus) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalSpeechResultAndWaitForTextAreaValue("Vega is a star",
                                               "Vega is a star");
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::KeyboardCode::VKEY_TAB, false, false, false, false)));
  WaitForRecognitionStopped();
  EXPECT_EQ("Vega is a star", GetTextAreaValue());
}

// Without the feature flag kExperimentalAccessibilityDictationCommands,
// commands should be treated like any other text.
IN_PROC_BROWSER_TEST_P(DictationExtensionTest, IgnoresCommands) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  std::string expected_text = "";
  for (const char* command : kEnglishDictationCommands) {
    expected_text += command;
    SendFinalSpeechResultAndWaitForTextAreaValue(command, expected_text);
  }
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

// Tests that the Dictation extension sets composition text when interim
// (non-finalized) speech results are returned, then commits text when the
// speech results are finalized.
IN_PROC_BROWSER_TEST_P(DictationExtensionTest, CompositionAndCommitText) {
  InstallMockInputContextHandler();

  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFakeSpeechResultAndWait(kFirstSpeechResult, /*is_final=*/false);
  WaitForCompositionText(kFirstSpeechResult16);
  SendFakeSpeechResultAndWait(kSecondSpeechResult, /*is_final=*/false);
  WaitForCompositionText(kSecondSpeechResult16);
  SendFinalFakeSpeechResultAndWait(kFinalSpeechResult);
  WaitForCommitText(kFinalSpeechResult16);
  ASSERT_EQ(2, GetUpdatePreeditTextCallCount());
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

// Tests behavior of the Dictation extension while ChromeVox is on.
IN_PROC_BROWSER_TEST_P(DictationExtensionTest,
                       CompositionAndCommitTextWithChromeVoxEnabled) {
  EnableChromeVox();
  EXPECT_TRUE(GetManager()->IsSpokenFeedbackEnabled());
  InstallMockInputContextHandler();

  // Toggle Dictation using AccessibilityManager. If we toggle Dictation with
  // a keystroke (Search + D), then it will be intercepted by ChromeVox and
  // Dictation won't be toggled.
  GetManager()->ToggleDictation();
  WaitForRecognitionStarted();
  SendFakeSpeechResultAndWait(kFirstSpeechResult, /*is_final=*/false);
  SendFakeSpeechResultAndWait(kSecondSpeechResult, /*is_final=*/false);
  // Finalized speech results should be committed.
  SendFinalFakeSpeechResultAndWait(kFinalSpeechResult);
  WaitForCommitText(kFinalSpeechResult16);
  // Dictation should not have set composition text if ChromeVox is on. This
  // helps reduce verbosity.
  ASSERT_EQ(0, GetUpdatePreeditTextCallCount());
  GetManager()->ToggleDictation();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationExtensionTest,
                       UserEndsDictationWhenChromeVoxEnabled) {
  EnableChromeVox();
  EXPECT_TRUE(GetManager()->IsSpokenFeedbackEnabled());
  InstallMockInputContextHandler();

  GetManager()->ToggleDictation();
  WaitForRecognitionStarted();
  SendFakeSpeechResultAndWait(kFinalSpeechResult, /*is_final=*/false);
  GetManager()->ToggleDictation();
  WaitForRecognitionStopped();

  WaitForCommitText(kFinalSpeechResult16);
  ASSERT_EQ(0, GetUpdatePreeditTextCallCount());
}

IN_PROC_BROWSER_TEST_P(DictationExtensionTest,
                       EntersInterimSpeechWhenToggledOff) {
  InstallMockInputContextHandler();

  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFakeSpeechResultAndWait(kFirstSpeechResult, /*is_final=*/false);
  WaitForCompositionText(kFirstSpeechResult16);
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
  WaitForCommitText(kFirstSpeechResult16);
  ASSERT_EQ(1, GetUpdatePreeditTextCallCount());
}

// Tests that composition and commit text are not updated if the user
// toggles dictation and no speech results are processed.
IN_PROC_BROWSER_TEST_P(DictationExtensionTest, UserEndsDictationBeforeSpeech) {
  InstallMockInputContextHandler();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
  ASSERT_EQ(0, GetUpdatePreeditTextCallCount());
  EXPECT_EQ(0, GetCommitTextCallCount());
}

// Ensures that the correct metrics are recorded when Dictation is toggled.
IN_PROC_BROWSER_TEST_P(DictationExtensionTest, Metrics) {
  base::HistogramTester histogram_tester_;
  bool on_device = GetParam() == speech::SpeechRecognitionType::kOnDevice;
  const char* metric_name = on_device ? kOnDeviceListeningDurationMetric
                                      : kNetworkListeningDurationMetric;
  HistogramWaiterOneShot waiter(metric_name);
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
  waiter.Wait();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Ensure that we recorded the correct locale.
  histogram_tester_.ExpectUniqueSample(/*name=*/kLocaleMetric,
                                       /*sample=*/base::PersistentHash("en-US"),
                                       /*expected_bucket_count=*/1);
  // Ensure that we recorded the type of speech recognition and listening
  // duration.
  if (on_device) {
    histogram_tester_.ExpectUniqueSample(/*name=*/kOnDeviceSpeechMetric,
                                         /*sample=*/true,
                                         /*expected_bucket_count=*/1);
    ASSERT_EQ(1,
              histogram_tester_.GetAllSamples(kOnDeviceListeningDurationMetric)
                  .size());
    // Ensure there are no metrics for the other type of speech recognition.
    ASSERT_EQ(0,
              histogram_tester_.GetAllSamples(kNetworkListeningDurationMetric)
                  .size());
  } else {
    histogram_tester_.ExpectUniqueSample(/*name=*/kOnDeviceSpeechMetric,
                                         /*sample=*/false,
                                         /*expected_bucket_count=*/1);
    ASSERT_EQ(1,
              histogram_tester_.GetAllSamples(kNetworkListeningDurationMetric)
                  .size());
    // Ensure there are no metrics for the other type of speech recognition.
    ASSERT_EQ(0,
              histogram_tester_.GetAllSamples(kOnDeviceListeningDurationMetric)
                  .size());
  }
}

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
    WaitForRecognitionStopped();
    DictationExtensionTest::TearDownOnMainThread();
  }

  void WaitForCaretBoundsChanged() {
    CaretBoundsChangedWaiter waiter(
        browser()->window()->GetNativeWindow()->GetHost()->GetInputMethod());
    waiter.Wait();
  }

  std::string GetClipboardText() {
    std::u16string text;
    ui::Clipboard::GetForCurrentThread()->ReadText(
        ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &text);
    return base::UTF16ToUTF8(text);
  }

  void SendFinalFakeSpeechResultAndWaitForSelectionChange(
      const std::string& result,
      content::WebContents* web_contents) {
    content::BoundingBoxUpdateWaiter waiter(web_contents);
    SendFinalFakeSpeechResultAndWait(result);
    waiter.Wait();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    Network,
    DictationCommandsExtensionTest,
    ::testing::Values(speech::SpeechRecognitionType::kNetwork));

INSTANTIATE_TEST_SUITE_P(
    OnDevice,
    DictationCommandsExtensionTest,
    ::testing::Values(speech::SpeechRecognitionType::kOnDevice));

IN_PROC_BROWSER_TEST_P(DictationCommandsExtensionTest, TypesCommands) {
  std::string expected_text = "";
  for (const char* command : kEnglishDictationCommands) {
    std::string type_command = "type ";
    expected_text += command;
    SendFinalSpeechResultAndWaitForTextAreaValue(type_command + command,
                                                 expected_text);
  }
}

IN_PROC_BROWSER_TEST_P(DictationCommandsExtensionTest, DeleteCharacter) {
  SendFinalSpeechResultAndWaitForTextAreaValue("Vega", "Vega");

  // Capitalization and whitespace shouldn't matter.
  SendFinalSpeechResultAndWaitForTextAreaValue(" Delete", "Veg");
  SendFinalSpeechResultAndWaitForTextAreaValue("delete ", "Ve");
  SendFinalSpeechResultAndWaitForTextAreaValue("  delete ", "V");
  SendFinalSpeechResultAndWaitForTextAreaValue("DELETE", "");
}

IN_PROC_BROWSER_TEST_P(DictationCommandsExtensionTest, MoveByCharacter) {
  SendFinalSpeechResultAndWaitForTextAreaValue("Lyra", "Lyra");

  SendFinalFakeSpeechResultAndWait("Move to the Previous character");
  WaitForCaretBoundsChanged();
  SendFinalSpeechResultAndWaitForTextAreaValue(" inserted ", "Lyr inserted a");
  SendFinalFakeSpeechResultAndWait("move TO the next character ");
  WaitForCaretBoundsChanged();
  SendFinalSpeechResultAndWaitForTextAreaValue(
      " is a constellation", "Lyr inserted a is a constellation");
}

IN_PROC_BROWSER_TEST_P(DictationCommandsExtensionTest, NewLineAndMoveByLine) {
  SendFinalSpeechResultAndWaitForTextAreaValue("Line 1", "Line 1");

  SendFinalSpeechResultAndWaitForTextAreaValue("new line", "Line 1\n");

  SendFinalSpeechResultAndWaitForTextAreaValue("Line 2", "Line 1\nLine 2");

  SendFinalFakeSpeechResultAndWait("Move to the previous line ");
  WaitForCaretBoundsChanged();
  SendFinalSpeechResultAndWaitForTextAreaValue("up", "Line 1up\nLine 2");

  SendFinalFakeSpeechResultAndWait("Move to the next line");
  WaitForCaretBoundsChanged();
  SendFinalSpeechResultAndWaitForTextAreaValue("down", "Line 1up\nLine 2down");
}

IN_PROC_BROWSER_TEST_P(DictationCommandsExtensionTest, UndoAndRedo) {
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

IN_PROC_BROWSER_TEST_P(DictationCommandsExtensionTest, SelectAllAndUnselect) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SendFinalSpeechResultAndWaitForTextAreaValue(
      "Vega is the brightest star in Lyra",
      "Vega is the brightest star in Lyra");
  SendFinalFakeSpeechResultAndWaitForSelectionChange("Select all",
                                                     web_contents);
  SendFinalSpeechResultAndWaitForTextAreaValue("delete", "");

  SendFinalSpeechResultAndWaitForTextAreaValue(
      "Vega is the fifth brightest star in the sky",
      "Vega is the fifth brightest star in the sky");
  SendFinalFakeSpeechResultAndWaitForSelectionChange("Select all",
                                                     web_contents);
  SendFinalFakeSpeechResultAndWaitForSelectionChange("Unselect", web_contents);
  SendFinalSpeechResultAndWaitForTextAreaValue(
      "!", "Vega is the fifth brightest star in the sky!");
}

IN_PROC_BROWSER_TEST_P(DictationCommandsExtensionTest, CutCopyPaste) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SendFinalSpeechResultAndWaitForTextAreaValue("Star", "Star");
  SendFinalFakeSpeechResultAndWaitForSelectionChange("Select all",
                                                     web_contents);
  ClipboardChangedWaiterOneShot copy_waiter;
  SendFinalFakeSpeechResultAndWait("Copy");
  copy_waiter.Wait();
  EXPECT_EQ("Star", GetClipboardText());
  SendFinalFakeSpeechResultAndWaitForSelectionChange("unselect", web_contents);

  SendFinalSpeechResultAndWaitForTextAreaValue("paste", "StarStar");

  SendFinalFakeSpeechResultAndWaitForSelectionChange("select ALL ",
                                                     web_contents);
  ClipboardChangedWaiterOneShot cut_waiter;
  SendFinalFakeSpeechResultAndWait("cut");
  cut_waiter.Wait();
  EXPECT_EQ("StarStar", GetClipboardText());
  WaitForTextAreaValue("");

  SendFinalSpeechResultAndWaitForTextAreaValue("  PaStE ", "StarStar");
}

// TODO(1266696): DictationCommandsExtensionTest.Help is failing on
// linux-chromeos-debug.
#if defined(OS_CHROMEOS)
#define MAYBE_Help DISABLED_Help
#else
#define MAYBE_Help Help
#endif
IN_PROC_BROWSER_TEST_P(DictationCommandsExtensionTest, MAYBE_Help) {
  SendFinalFakeSpeechResultAndWait("HELP");
  // Opening a new tab with the help center article toggles Dictation off.
  WaitForRecognitionStopped();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(web_contents->GetVisibleURL().spec().rfind(
                  "https://support.google.com/chromebook", /*pos=*/0) != 0);
}

// TODO(crbug.com/1264544): Test looking at gn args has pumpkin and does
// repeats.

}  // namespace ash
