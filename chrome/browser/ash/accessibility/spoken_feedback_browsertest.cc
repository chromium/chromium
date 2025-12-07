// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/spoken_feedback_browsertest.h"

#include <queue>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/accessibility/ui/accessibility_confirmation_dialog.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/public/cpp/screen_backlight.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/accessibility/caption_bubble_context_browser.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/automation_test_utils.h"
#include "chrome/browser/ash/accessibility/fullscreen_magnifier_test_helper.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_display_private_api.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/input_method/candidate_window_view.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/common/constants.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

using KeyEvent = ::extensions::api::braille_display_private::KeyEvent;
using KeyCommand = ::extensions::api::braille_display_private::KeyCommand;

namespace ash {

namespace {

const char* kChromeVoxPerformCommandMetric =
    "Accessibility.ChromeVox.PerformCommand";
const base::TimeDelta kExpectedPhoneticSpeechAndHintDelay = base::Seconds(1);

}  // namespace

LoggedInSpokenFeedbackTest::LoggedInSpokenFeedbackTest()
    : animation_mode_(gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}

LoggedInSpokenFeedbackTest::~LoggedInSpokenFeedbackTest() = default;

void LoggedInSpokenFeedbackTest::SetUpInProcessBrowserTestFixture() {
  AccessibilityManager::SetBrailleControllerForTest(&braille_controller_);
  AccessibilityFeatureBrowserTest::SetUpInProcessBrowserTestFixture();
}

void LoggedInSpokenFeedbackTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  AccessibilityFeatureBrowserTest::SetUpCommandLine(command_line);

  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  if (GetParam().manifest_version() == ManifestVersion::kTwo) {
    disabled_features.push_back(::features::kAccessibilityManifestV3ChromeVox);
  } else if (GetParam().manifest_version() == ManifestVersion::kThree) {
    enabled_features.push_back(::features::kAccessibilityManifestV3ChromeVox);
  }
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

void LoggedInSpokenFeedbackTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  event_generator_ = std::make_unique<ui::test::EventGenerator>(
      Shell::Get()->GetPrimaryRootWindow());
  chromevox_test_utils_ = std::make_unique<ChromeVoxTestUtils>();
  AccessibilityFeatureBrowserTest::SetUpOnMainThread();
}

void LoggedInSpokenFeedbackTest::TearDownOnMainThread() {
  event_generator_.reset();
  chromevox_test_utils_.reset();
  AccessibilityManager::SetBrailleControllerForTest(nullptr);
  // Unload the ChromeVox extension so the browser doesn't try to respond to
  // in-flight requests during test shutdown. https://crbug.com/923090
  AccessibilityManager::Get()->EnableSpokenFeedback(false);
  AutomationManagerAura::GetInstance()->Disable();
}

void LoggedInSpokenFeedbackTest::SendKeyPress(ui::KeyboardCode key) {
  ui::test::EmulateFullKeyPressReleaseSequence(
      event_generator_.get(), key,
      /*control=*/false, /*shift=*/false, /*alt=*/false, /*command=*/false);
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithControl(ui::KeyboardCode key) {
  ui::test::EmulateFullKeyPressReleaseSequence(
      event_generator_.get(), key, /*control=*/true, /*shift=*/false,
      /*alt=*/false, /*command=*/false);
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithControlAndAlt(
    ui::KeyboardCode key) {
  ui::test::EmulateFullKeyPressReleaseSequence(event_generator_.get(), key,
                                               /*control=*/true,
                                               /*shift=*/false,
                                               /*alt=*/true, /*command=*/false);
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithControlAndShift(
    ui::KeyboardCode key) {
  ui::test::EmulateFullKeyPressReleaseSequence(event_generator_.get(), key,
                                               /*control=*/true, /*shift=*/true,
                                               /*alt=*/false,
                                               /*command=*/false);
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithShift(ui::KeyboardCode key) {
  ui::test::EmulateFullKeyPressReleaseSequence(
      event_generator_.get(), key, /*control=*/false, /*shift=*/true,
      /*alt=*/false, /*command=*/false);
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithAltAndShift(
    ui::KeyboardCode key) {
  ui::test::EmulateFullKeyPressReleaseSequence(
      event_generator_.get(), key, /*control=*/false, /*shift=*/true,
      /*alt=*/true, /*command=*/false);
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithSearchAndShift(
    ui::KeyboardCode key) {
  ui::test::EmulateFullKeyPressReleaseSequence(event_generator_.get(), key,
                                               /*control=*/false,
                                               /*shift=*/true, /*alt=*/false,
                                               /*command=*/true);
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithSearch(ui::KeyboardCode key) {
  ui::test::EmulateFullKeyPressReleaseSequence(
      event_generator_.get(), key, /*control=*/false, /*shift=*/false,
      /*alt=*/false, /*command=*/true);
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithSearchAndControl(
    ui::KeyboardCode key) {
  ui::test::EmulateFullKeyPressReleaseSequence(event_generator_.get(), key,
                                               /*control=*/true,
                                               /*shift=*/false, /*alt=*/false,
                                               /*command=*/true);
}

void LoggedInSpokenFeedbackTest::SendKeyPressWithSearchAndControlAndShift(
    ui::KeyboardCode key) {
  ui::test::EmulateFullKeyPressReleaseSequence(event_generator_.get(), key,
                                               /*control=*/true, /*shift=*/true,
                                               /*alt=*/false, /*command=*/true);
}

void LoggedInSpokenFeedbackTest::SendStickyKeyCommand() {
  // To avoid flakes in sending keys, execute the command directly in js.
  chromevox_test_utils()->ExecuteCommandHandlerCommand("toggleStickyMode");
}

void LoggedInSpokenFeedbackTest::SendMouseMoveTo(const gfx::Point& location) {
  event_generator_->MoveMouseTo(location.x(), location.y());
}

void LoggedInSpokenFeedbackTest::SetMouseSourceDeviceId(int id) {
  event_generator_->set_mouse_source_device_id(id);
}

bool LoggedInSpokenFeedbackTest::PerformAcceleratorAction(
    AcceleratorAction action) {
  return AcceleratorController::Get()->PerformActionIfEnabled(action, {});
}

void LoggedInSpokenFeedbackTest::StablizeChromeVoxState() {
  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
        <button autofocus>Click me</button>)"));
  });
  sm()->ExpectSpeech("Click me");
}

INSTANTIATE_TEST_SUITE_P(
    ManifestV2,
    LoggedInSpokenFeedbackTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3,
    LoggedInSpokenFeedbackTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree)));

// Flaky test, crbug.com/1081563
IN_PROC_BROWSER_TEST_P(LoggedInSpokenFeedbackTest, DISABLED_AddBookmark) {
  chromevox_test_utils()->EnableChromeVox();

  // Open the bookmarks bar.
  sm()->Call([this]() { SendKeyPressWithControlAndShift(ui::VKEY_B); });

  // Create a bookmark with title "foo".
  sm()->Call([this]() { SendKeyPressWithControl(ui::VKEY_D); });

  sm()->ExpectSpeech("Bookmark name");
  sm()->ExpectSpeech("about:blank");
  sm()->ExpectSpeech("selected");
  sm()->ExpectSpeech("Edit text");
  sm()->ExpectSpeech("Bookmark added");
  sm()->ExpectSpeech("Dialog");
  sm()->ExpectSpeech("Bookmark added, window");

  sm()->Call([this]() {
    SendKeyPress(ui::VKEY_F);
    SendKeyPress(ui::VKEY_O);
    SendKeyPress(ui::VKEY_O);
  });
  sm()->ExpectSpeech("F");
  sm()->ExpectSpeech("O");
  sm()->ExpectSpeech("O");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm()->ExpectSpeech("Bookmark folder");
  sm()->ExpectSpeech("Bookmarks bar");
  sm()->ExpectSpeech("Button");
  sm()->ExpectSpeech("has pop up");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm()->ExpectSpeech("More…");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm()->ExpectSpeech("Remove");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm()->ExpectSpeech("Done");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_RETURN); });
  // Focus goes back to window.
  sm()->ExpectSpeechPattern("about:blank*");

  // Focus bookmarks bar and listen for "foo".
  sm()->Call([this]() {
    // Focus bookmarks bar.
    SendKeyPressWithAltAndShift(ui::VKEY_B);
  });
  sm()->ExpectSpeech("foo");
  sm()->ExpectSpeech("Button");
  sm()->ExpectSpeech("Bookmarks");
  sm()->ExpectSpeech("Tool bar");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(LoggedInSpokenFeedbackTest, ChromeVoxSpeaksIntro) {
  base::HistogramTester histogram_tester;
  chromevox_test_utils()->EnableChromeVox(/*check_for_intro=*/false);
  sm()->ExpectSpeech("ChromeVox spoken feedback is ready");
  sm()->Replay();

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  if (histogram_tester
          .GetAllSamples("Accessibility.ChromeVox.StartUpSpeechDelay")
          .size() == 0) {
    base::StatisticsRecorder::HistogramWaiter(
        "Accessibility.ChromeVox.StartUpSpeechDelay").Wait();
  }
}

// Test Learn Mode by pressing a few keys in Learn Mode. Only available while
// logged in.
IN_PROC_BROWSER_TEST_P(LoggedInSpokenFeedbackTest, LearnModeHardwareKeys) {
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this]() {
    chromevox_test_utils()->ExecuteCommandHandlerCommand("showLearnModePage");
  });
  sm()->ExpectSpeechPattern(
      "Press a qwerty key, refreshable braille key, or touch gesture to learn "
      "*");

  // These are the default top row keys and their descriptions which live in
  // ChromeVox.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_F1); });
  sm()->ExpectSpeech("back");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_F2); });
  sm()->ExpectSpeech("forward");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_F3); });
  sm()->ExpectSpeech("refresh");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_F4); });
  sm()->ExpectSpeech("toggle full screen");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_F5); });
  sm()->ExpectSpeech("show windows");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_F6); });
  sm()->ExpectSpeech("Brightness down");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_F7); });
  sm()->ExpectSpeech("Brightness up");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_F8); });
  sm()->ExpectSpeech("volume mute");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_F9); });
  sm()->ExpectSpeech("volume down");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_F10); });
  sm()->ExpectSpeech("volume up");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(LoggedInSpokenFeedbackTest, LearnModeEscapeWithGesture) {
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this]() {
    chromevox_test_utils()->ExecuteCommandHandlerCommand("showLearnModePage");
  });
  sm()->ExpectSpeechPattern(
      "Press a qwerty key, refreshable braille key, or touch gesture to learn "
      "*");

  sm()->Call([]() {
    AccessibilityManager::Get()->HandleAccessibilityGesture(
        ax::mojom::Gesture::kSwipeLeft2, gfx::PointF());
  });
  sm()->ExpectSpeech("Swipe two fingers left");
  sm()->ExpectSpeech("Escape");
  sm()->ExpectSpeech("Stopping Learn Mode");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(LoggedInSpokenFeedbackTest,
                       LearnModePressEscapeTwiceToExit) {
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this]() {
    chromevox_test_utils()->ExecuteCommandHandlerCommand("showLearnModePage");
  });
  sm()->ExpectSpeechPattern(
      "Press a qwerty key, refreshable braille key, or touch gesture to learn "
      "*");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_ESCAPE); });
  sm()->ExpectSpeech("Escape");
  sm()->ExpectSpeech("Press escape again to exit Learn Mode");

  // Pressing a different key means the next escape key will not exit learn
  // mode, it has to be pressed twice in a row.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_K); });
  sm()->ExpectSpeech("K");

  // Press escape again, it warns about exiting again.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_ESCAPE); });
  sm()->ExpectSpeech("Escape");
  sm()->ExpectSpeech("Press escape again to exit Learn Mode");

  // Press it a second time in a row, should actually exit.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_ESCAPE); });
  sm()->ExpectSpeech("Escape");
  sm()->ExpectSpeech("Stopping Learn Mode");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(LoggedInSpokenFeedbackTest, OpenLogPage) {
  // Enabling earcon logging should not crash ChromeVox at startup
  // (see b/318531241).
  AccessibilityManager::Get()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityChromeVoxEnableEarconLogging, true);
  chromevox_test_utils()->EnableChromeVox();
  StablizeChromeVoxState();

  // Open the log page.
  sm()->Call([this]() {
    SendKeyPressWithSearch(ui::VKEY_O);
    SendKeyPress(ui::VKEY_W);
  });
  sm()->ExpectSpeech("chromevox-log");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(LoggedInSpokenFeedbackTest,
                       CheckChromeVoxPerformCommandMetric) {
  chromevox_test_utils()->EnableChromeVox();
  base::HistogramTester histogram_tester;

  // Command.ANNOUNCE_BATTERY_DESCRIPTION
  sm()->Call([this]() {
    chromevox_test_utils()->ExecuteCommandHandlerCommand(
        "announceBatteryDescription");
  });
  sm()->ExpectSpeechPattern("*");
  // Command.NEXT_OBJECT
  sm()->Call([this]() {
    chromevox_test_utils()->ExecuteCommandHandlerCommand("nextObject");
  });
  sm()->ExpectSpeechPattern("*");
  // Command.DECREASE_TTS_RATE
  sm()->Call([this]() {
    chromevox_test_utils()->ExecuteCommandHandlerCommand("decreaseTtsRate");
  });
  sm()->ExpectSpeechPattern("*");
  // Command.NEXT_BUTTON
  sm()->Call([this]() {
    chromevox_test_utils()->ExecuteCommandHandlerCommand("nextButton");
  });
  sm()->ExpectSpeechPattern("*");
  // Command.HELP
  sm()->Call([this]() {
    chromevox_test_utils()->ExecuteCommandHandlerCommand("help");
  });
  sm()->ExpectSpeechPattern("*");
  sm()->Replay();

  histogram_tester.ExpectBucketCount(
      kChromeVoxPerformCommandMetric,
      0 /*ChromeVoxCommand.ANNOUNCE_BATTERY_DESCRIPTION*/, 1);
  histogram_tester.ExpectBucketCount(kChromeVoxPerformCommandMetric,
                                     81 /*ChromeVoxCommand.NEXT_OBJECT*/, 1);
  histogram_tester.ExpectBucketCount(kChromeVoxPerformCommandMetric,
                                     12 /*ChromeVoxCommand.DECREASE_TTS_RATE*/,
                                     1);
  histogram_tester.ExpectBucketCount(kChromeVoxPerformCommandMetric,
                                     55 /*ChromeVoxCommand.NEXT_BUTTON*/, 1);
  histogram_tester.ExpectBucketCount(kChromeVoxPerformCommandMetric,
                                     36 /*ChromeVoxCommand.HELP*/, 1);
  histogram_tester.ExpectTotalCount(kChromeVoxPerformCommandMetric, 5);
}

class CaptionSpokenFeedbackTest : public LoggedInSpokenFeedbackTest {
 protected:
  ~CaptionSpokenFeedbackTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoggedInSpokenFeedbackTest::SetUpCommandLine(command_line);

    scoped_feature_list_.InitWithFeatures(
        {ash::features::kOnDeviceSpeechRecognition,
         ::features::kAccessibilityCaptionsOnBrailleDisplay},
        {});
  }

  void SetCaptionText(const std::string& text) {
    // The UI is only created after SODA is installed.
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
        speech::LanguageCode::kEnUs);
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();

    ::captions::LiveCaptionController* live_caption_controller =
        ::captions::LiveCaptionControllerFactory::GetForProfile(
            AccessibilityManager::Get()->profile());
    live_caption_controller->DispatchTranscription(
        /*web_contents=*/nullptr, GetCaptionBubbleContext(),
        media::SpeechRecognitionResult(text, /*is_final=*/false));
  }

  ::captions::CaptionBubbleContextBrowser* GetCaptionBubbleContext() {
    if (!caption_bubble_context_) {
      caption_bubble_context_ = ::captions::CaptionBubbleContextBrowser::Create(
          browser()->tab_strip_model()->GetActiveWebContents());
    }
    return caption_bubble_context_.get();
  }

  void SendBrailleKeyEvent(KeyCommand key_command) {
    KeyEvent key_event;
    key_event.command = key_command;
    extensions::BrailleDisplayPrivateAPI(GetProfile())
        .OnBrailleKeyEvent(key_event);
  }

 private:
  std::unique_ptr<::captions::CaptionBubbleContextBrowser>
      caption_bubble_context_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ManifestV2,
    CaptionSpokenFeedbackTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3,
    CaptionSpokenFeedbackTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree)));

IN_PROC_BROWSER_TEST_P(CaptionSpokenFeedbackTest, ToggleCaptions) {
  PrefChangeRegistrar change_observer;
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this, &change_observer]() {
    change_observer.Init(AccessibilityManager::Get()->profile()->GetPrefs());
    change_observer.Add(::prefs::kLiveCaptionEnabled,
                        base::BindLambdaForTesting([this, &change_observer]() {
                          SetCaptionText("Hello World");
                          change_observer.Remove(::prefs::kLiveCaptionEnabled);
                        }));
    chromevox_test_utils()->ExecuteCommandHandlerCommand("toggleCaptions");
  });
  sm()->ExpectSpeech("Hello World");
  sm()->Call([this, &change_observer]() {
    chromevox_test_utils()->ExecuteCommandHandlerCommand("toggleCaptions");
    change_observer.Add(::prefs::kLiveCaptionEnabled,
                        base::BindLambdaForTesting([this]() {
                          SetCaptionText("Goodbye World");
                          chromevox_test_utils()->ExecuteCommandHandlerCommand(
                              "nextLine");
                        }));
  });
  sm()->ExpectNextSpeechIsNotPattern("Goodbye World");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(CaptionSpokenFeedbackTest, CaptionsNotToggled) {
  PrefChangeRegistrar change_observer;
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this, &change_observer]() {
    change_observer.Init(AccessibilityManager::Get()->profile()->GetPrefs());
    change_observer.Add(::prefs::kLiveCaptionEnabled,
                        base::BindLambdaForTesting(
                            [this]() { SetCaptionText("Hello World"); }));
  });
  sm()->Call([this]() {
    chromevox_test_utils()->ExecuteCommandHandlerCommand("nextLine");
  });
  sm()->ExpectNextSpeechIsNotPattern("Hello World");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(CaptionSpokenFeedbackTest, CaptionsChanged) {
  std::string captions = "To be, or not to be";
  PrefChangeRegistrar change_observer;
  chromevox_test_utils()->EnableChromeVox();

  AutomationTestUtils test_utils(extension_misc::kChromeVoxExtensionId);
  sm()->Call([&test_utils]() { test_utils.SetUpTestSupport(); });

  sm()->Call([this, &change_observer, &captions]() {
    // Use braille captions as an approximation of a braille display.
    chromevox_test_utils()->ExecuteCommandHandlerCommand(
        "toggleBrailleCaptions");

    // Set the caption text only once live captions is enabled.
    change_observer.Init(AccessibilityManager::Get()->profile()->GetPrefs());
    change_observer.Add(::prefs::kLiveCaptionEnabled,
                        base::BindLambdaForTesting(
                            [this, &captions]() { SetCaptionText(captions); }));
    chromevox_test_utils()->ExecuteCommandHandlerCommand("toggleCaptions");
  });
  sm()->ExpectSpeechPattern("To be*");
  sm()->Call([this, &captions, &test_utils]() {
    captions += ", that is the question";
    SetCaptionText(captions);
    test_utils.WaitForChildrenChangedEvent();

    SendBrailleKeyEvent(KeyCommand::kPanRight);
  });
  sm()->ExpectSpeechPattern("*that is*");
  sm()->Call([this, &captions, &test_utils]() {
    captions += ": Whether 'tis nobler in the";
    SetCaptionText(captions);
    test_utils.WaitForChildrenChangedEvent();

    SendBrailleKeyEvent(KeyCommand::kPanRight);
  });
  sm()->ExpectSpeechPattern("*Whether*");

  sm()->Replay();
}

class NotificationCenterSpokenFeedbackTest : public LoggedInSpokenFeedbackTest {
 protected:
  NotificationCenterSpokenFeedbackTest() = default;
  ~NotificationCenterSpokenFeedbackTest() override = default;

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
    ManifestV2,
    NotificationCenterSpokenFeedbackTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3,
    NotificationCenterSpokenFeedbackTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree)));

// Tests the spoken feedback text when using the notification center accelerator
// to navigate to the notification center.
IN_PROC_BROWSER_TEST_P(NotificationCenterSpokenFeedbackTest,
                       NavigateNotificationCenter) {
  chromevox_test_utils()->EnableChromeVox();

  // Add a notification so that the notification center tray is visible.
  test_api()->AddNotification();
  ASSERT_TRUE(test_api()->IsTrayShown());

  // Press the accelerator that toggles the notification center.
  sm()->Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(
        AcceleratorAction::kToggleMessageCenterBubble));
  });

  // Verify the spoken feedback text.
  sm()->ExpectSpeech("Notification Center");
  sm()->Replay();
}

// Tests that clicking the notification center tray does not crash when spoken
// feedback is enabled.
IN_PROC_BROWSER_TEST_P(NotificationCenterSpokenFeedbackTest, OpenBubble) {
  // Enable spoken feedback and add a notification to ensure the tray is
  // visible.
  chromevox_test_utils()->EnableChromeVox();
  test_api()->AddNotification();
  ASSERT_TRUE(test_api()->IsTrayShown());

  // Click on the tray and verify the bubble shows up.
  sm()->Call([this]() {
    test_api()->ToggleBubble();
    EXPECT_TRUE(test_api()->GetWidget()->IsActive());
    EXPECT_TRUE(test_api()->IsBubbleShown());
  });
  sm()->ExpectSpeech("Notification Center");

  sm()->Replay();
}

// Tests that an incoming silent notification (i.e. a notification that goes
// straight to the notification center without generating a popup) generates an
// accessibility announcement.
IN_PROC_BROWSER_TEST_P(NotificationCenterSpokenFeedbackTest,
                       SilentNotification) {
  // Enable spoken feedback.
  chromevox_test_utils()->EnableChromeVox();

  // Add a silent notification while the notification center is not showing.
  sm()->Call([this]() {
    ASSERT_FALSE(
        message_center::MessageCenter::Get()->IsMessageCenterVisible());
    test_api()->AddLowPriorityNotification();
  });

  // Verify that the silent notification was announced.
  auto expected_announcement = l10n_util::GetStringFUTF16Int(
      IDS_ASH_MESSAGE_CENTER_SILENT_NOTIFICATION_ANNOUNCEMENT, 1);
  sm()->ExpectSpeech(base::UTF16ToUTF8(expected_announcement));

  // Add another silent notification, this time while the notification center is
  // showing.
  sm()->Call([this]() {
    test_api()->ToggleBubble();
    ASSERT_TRUE(message_center::MessageCenter::Get()->IsMessageCenterVisible());
    test_api()->AddLowPriorityNotification();
  });

  // Verify that the silent notification was announced.
  expected_announcement = l10n_util::GetStringFUTF16Int(
      IDS_ASH_MESSAGE_CENTER_SILENT_NOTIFICATION_ANNOUNCEMENT, 2);
  sm()->ExpectSpeech(base::UTF16ToUTF8(expected_announcement));

  sm()->Replay();
}

class SpokenFeedbackTest : public LoggedInSpokenFeedbackTest {
 protected:
  SpokenFeedbackTest() = default;
  virtual ~SpokenFeedbackTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoggedInSpokenFeedbackTest::SetUpCommandLine(command_line);

    if (GetParam().variant() == kTestAsGuestUser) {
      command_line->AppendSwitch(switches::kGuestSession);
      command_line->AppendSwitch(::switches::kIncognito);
      command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
      command_line->AppendSwitchASCII(
          switches::kLoginUser, user_manager::GuestAccountId().GetUserEmail());
    }
  }

  void NavigateToMathPage() {
    sm()->Call([this]() {
      NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
          <button autofocus>start</button>
          <math>
            <mfrac>
              <mrow>
                <mi>d</mi>
                <mi>y</mi>
              </mrow>
              <mrow>
                <mi>d</mi>
                <mi>x</mi>
              </mrow>
            </mfrac>
            <mo>=</mo>
          </math>
          <p>end</p>)"));
    });
    sm()->ExpectSpeech("start");
  }
};

INSTANTIATE_TEST_SUITE_P(
    ManifestV2NormalUser,
    SpokenFeedbackTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo,
                                               kTestAsNormalUser)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV2GuestUser,
    SpokenFeedbackTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo,
                                               kTestAsGuestUser)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3NormalUser,
    SpokenFeedbackTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree,
                                               kTestAsNormalUser)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3GuestUser,
    SpokenFeedbackTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree,
                                               kTestAsGuestUser)));

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, EnableSpokenFeedback) {
  chromevox_test_utils()->EnableChromeVox();
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, FocusToolbar) {
  chromevox_test_utils()->EnableChromeVox();
  // Wait for the browser to show up.
  StablizeChromeVoxState();
  sm()->Call([this]() { SendKeyPressWithAltAndShift(ui::VKEY_T); });
  // The back button should become focused.
  sm()->ExpectSpeech("Back");
  sm()->Replay();
}

// TODO(crbug.com/1065235): flaky.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_TypeInOmnibox) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([this]() {
    NavigateToUrl(GURL("data:text/html;charset=utf-8,<p>unused</p>"));
  });

  sm()->Call([this]() { SendKeyPressWithControl(ui::VKEY_L); });
  sm()->ExpectSpeech("Address and search bar");

  sm()->Call([this]() {
    // Select all the text.
    SendKeyPressWithControl(ui::VKEY_A);

    // Type x, y, and z.
    SendKeyPress(ui::VKEY_X);
    SendKeyPress(ui::VKEY_Y);
    SendKeyPress(ui::VKEY_Z);
  });
  sm()->ExpectSpeech("X");
  sm()->ExpectSpeech("Y");
  sm()->ExpectSpeech("Z");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_BACK); });
  sm()->ExpectSpeech("Z");

  // Auto completions.
  sm()->ExpectSpeech("xy search");
  sm()->ExpectSpeech("List item");
  sm()->ExpectSpeech("1 of 1");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, FocusShelf) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::kFocusShelf));
  });
  sm()->ExpectSpeechPattern("Launcher");
  sm()->ExpectSpeech("Button");
  sm()->ExpectSpeech("Shelf");
  sm()->ExpectSpeech("Tool bar");
  sm()->ExpectSpeech(", window");
  sm()->ExpectSpeech("Press Search plus Space to activate");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm()->ExpectSpeechPattern("Button");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, SelectChromeVoxMenuItem) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_PERIOD); });
  sm()->ExpectSpeech("ChromeVox Panel");
  sm()->ExpectSpeech("Search");
  sm()->Call([this]() {
    SendKeyPress(ui::VKEY_RIGHT);
    SendKeyPress(ui::VKEY_RIGHT);
  });
  sm()->ExpectSpeech("Speech");
  sm()->ExpectSpeech("Announce Current Battery Status");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_RETURN); });
  sm()->ExpectSpeechPattern("Battery at* percent*");

  sm()->Replay();
}

// Verifies that pressing right arrow button with search button should move
// focus to the next ShelfItem instead of the last one
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ShelfIconFocusForward) {
  const std::string title("MockApp");
  ChromeShelfController* controller = ChromeShelfController::instance();

  // Add the ShelfItem to the ShelfModel after enabling the ChromeVox. Because
  // when an extension is enabled, the ShelfItems which are not recorded as
  // pinned apps in user preference will be removed.
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([controller, title]() {
    controller->CreateAppItem(
        std::make_unique<AppShortcutShelfItemController>(ShelfID("FakeApp")),
        STATUS_CLOSED, /*pinned=*/true, base::ASCIIToUTF16(title));
  });

  // Focus on the shelf.
  sm()->Call(
      [this]() { PerformAcceleratorAction(AcceleratorAction::kFocusShelf); });
  sm()->ExpectSpeech("Launcher");
  sm()->ExpectSpeech("Button");
  sm()->ExpectSpeech("Shelf");
  sm()->ExpectSpeech("Tool bar");

  // Verifies that pressing right key with search key should move the focus of
  // ShelfItem correctly.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  // Chromium or Google Chrome button here (not being tested).
  sm()->ExpectSpeech("Button");
  sm()->ExpectSpeech("Shelf");
  sm()->ExpectSpeech("Tool bar");

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("MockApp");
  sm()->ExpectSpeech("Button");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, NavigateSpeechMenu) {
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
        <a href="https://google.com" autofocus>Link to Google</a>
        <p>Text after link</p>)"));
  });
  sm()->ExpectSpeech("Link to Google");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_PERIOD); });
  sm()->ExpectSpeech("Search the menus");
  sm()->Call([this]() {
    SendKeyPress(ui::VKEY_RIGHT);
    SendKeyPress(ui::VKEY_RIGHT);
  });
  sm()->ExpectSpeech("Speech Menu");
  sm()->ExpectSpeech("Announce Current Battery Status");
  sm()->ExpectSpeechPattern("Menu item 1 of *");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeechPattern("Menu item 2 of *");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeechPattern("Menu item 3 of *");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeechPattern("Menu item 4 of *");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeechPattern("Menu item 5 of *");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeechPattern("Menu item 6 of *");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_UP); });
  sm()->ExpectSpeech("Announce The URL Behind A Link");
  sm()->ExpectSpeechPattern("Menu item 5 of *");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_SPACE); });
  sm()->ExpectSpeech("Link URL: https colon slash slash google.com slash");

  // Verify that the menu has closed and we are back in the web contents.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("Text after link");

  sm()->Replay();
}

// TODO(https://crbug.com/1486666): Fix flakiness
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_OpenContextMenu) {
  chromevox_test_utils()->EnableChromeVox();
  StablizeChromeVoxState();
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_M); });
  sm()->ExpectSpeech("menu opened");
  // Close the menu
  sm()->Call([this]() { SendKeyPress(ui::VKEY_ESCAPE); });
  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
            <button autofocus>I'm a button</button>)"));
  });
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_M); });
  sm()->ExpectSpeech("menu opened");

  sm()->Replay();
}

// Verifies that speaking text under mouse works for Shelf button and voice
// announcements should not be stacked when mouse goes over many Shelf buttons
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, SpeakingTextUnderMouseForShelfItem) {
  SetMouseSourceDeviceId(1);
  AccessibilityManager::Get()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityChromeVoxSpeakTextUnderMouse, true);
  // Add the ShelfItem to the ShelfModel after enabling the ChromeVox. Because
  // when an extension is enabled, the ShelfItems which are not recorded as
  // pinned apps in user preference will be removed.
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([this]() {
    // Add three Shelf buttons. Wait for the change on ShelfModel to reach ash.
    ChromeShelfController* controller = ChromeShelfController::instance();
    const std::string title("MockApp");
    const std::string id("FakeApp");
    const int insert_app_num = 3;
    for (int i = 0; i < insert_app_num; i++) {
      std::string app_title = title + base::NumberToString(i);
      std::string app_id = id + base::NumberToString(i);
      controller->CreateAppItem(
          std::make_unique<AppShortcutShelfItemController>(ShelfID(app_id)),
          STATUS_CLOSED, /*pinned=*/true, base::ASCIIToUTF16(app_title));
    }

    // Focus on the Shelf because voice text for focusing on Shelf is fixed.
    // Wait until voice announcements are finished.
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::kFocusShelf));
  });
  sm()->ExpectSpeechPattern("Launcher");

  // Hover mouse on the Shelf button. Verifies that text under mouse is spoken.
  sm()->Call([this]() {
    ShelfView* shelf_view =
        Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
            ->shelf_widget()
            ->shelf_view_for_testing();
    const int first_app_index = shelf_view->model()->GetItemIndexForType(
        ShelfItemType::TYPE_PINNED_APP);
    SendMouseMoveTo(shelf_view->view_model()
                        ->view_at(first_app_index)
                        ->GetBoundsInScreen()
                        .CenterPoint());
  });

  sm()->ExpectSpeechPattern("MockApp*");
  sm()->ExpectSpeech("Button");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, SpeakingTextUnderSynthesizedMouse) {
  AccessibilityManager::Get()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityChromeVoxSpeakTextUnderMouse, true);

  chromevox_test_utils()->EnableChromeVox();

  AutomationTestUtils test_utils(extension_misc::kChromeVoxExtensionId);
  sm()->Call([&test_utils]() {
    test_utils.SetUpTestSupport();
    // Enable the function of speaking text under mouse.
    EventRewriterController::Get()->SetSendMouseEvents(true);
  });

  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
            <button id="b1" autofocus>First</button>
            <button id="b2">Second</button>
            <button id="b3">Third</button>
        )"));
  });
  sm()->ExpectSpeech("First");
  sm()->Call([this, &test_utils]() {
    gfx::Rect b2_bounds = test_utils.GetNodeBoundsInRoot("Second", "button");
    SendMouseMoveTo(b2_bounds.CenterPoint());
  });
  sm()->ExpectSpeech("Second");
  sm()->Call([this, &test_utils]() {
    gfx::Rect b3_bounds = test_utils.GetNodeBoundsInRoot("Third", "button");
    SendMouseMoveTo(b3_bounds.CenterPoint());
  });
  sm()->ExpectSpeech("Third");

  sm()->Replay();
}

// Verifies that an announcement is triggered when focusing a ShelfItem with a
// notification badge shown.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ShelfNotificationBadgeAnnouncement) {
  chromevox_test_utils()->EnableChromeVox();

  // Create and add a test app to the shelf model.
  ShelfItem item;
  item.id = ShelfID("TestApp");
  item.title = u"TestAppTitle";
  item.type = ShelfItemType::TYPE_APP;
  ShelfModel::Get()->Add(item,
                         std::make_unique<TestShelfItemDelegate>(item.id));

  // Set the notification badge to be shown for the test app.
  ShelfModel::Get()->UpdateItemNotification("TestApp", /*has_badge=*/true);

  // Focus on the shelf.
  sm()->Call(
      [this]() { PerformAcceleratorAction(AcceleratorAction::kFocusShelf); });
  sm()->ExpectSpeech("Launcher");
  sm()->ExpectSpeech("Button");
  sm()->ExpectSpeech("Shelf");
  sm()->ExpectSpeech("Tool bar");

  // Press right key twice to focus the test app.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("TestAppTitle");
  sm()->ExpectSpeech("Button");

  // Check that when a shelf app button with a notification badge is focused,
  // the correct announcement occurs.
  sm()->ExpectSpeech("TestAppTitle requests your attention.");

  sm()->Replay();
}

// Verifies that an announcement is triggered when focusing a paused app
// ShelfItem.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       ShelfPausedAppIconBadgeAnnouncement) {
  chromevox_test_utils()->EnableChromeVox();

  std::string app_id = "TestApp";

  // Set the app status as paused;
  apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kWeb, app_id);
  app->readiness = apps::Readiness::kReady;
  app->paused = true;

  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  apps::AppServiceProxyFactory::GetForProfile(GetProfile())
      ->OnApps(std::move(apps), apps::AppType::kWeb,
               false /* should_notify_initialized */);

  // Create and add a test app to the shelf model.
  ShelfItem item;
  item.id = ShelfID(app_id);
  item.title = u"TestAppTitle";
  item.type = ShelfItemType::TYPE_APP;
  item.app_status = AppStatus::kPaused;
  ShelfModel::Get()->Add(item,
                         std::make_unique<TestShelfItemDelegate>(item.id));

  // Focus on the shelf.
  sm()->Call(
      [this]() { PerformAcceleratorAction(AcceleratorAction::kFocusShelf); });
  sm()->ExpectSpeech("Launcher");
  sm()->ExpectSpeech("Button");
  sm()->ExpectSpeech("Shelf");
  sm()->ExpectSpeech("Tool bar");

  // Press right key twice to focus the test app.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("TestAppTitle");
  sm()->ExpectSpeech("Button");

  // Check that when a paused app shelf item is focused, the correct
  // announcement occurs.
  sm()->ExpectSpeech("Paused");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ShowHeadingList) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
        <h1>Page Title</h1>
        <h2>First Section</h2>
        <h3>Sub-category</h3>
        <p>Text</p>
        <h3>Second sub-category<h3>
        <button autofocus>Next page</button>)"));
  });
  sm()->ExpectSpeech("Next page");
  sm()->Call([this]() { SendKeyPressWithSearchAndControl(ui::VKEY_H); });
  sm()->ExpectSpeech("Heading Menu");
  sm()->ExpectSpeechPattern("Page Title Heading 1 Menu item 1 of *");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeechPattern("First Section Heading 2 Menu item 2 of *");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeechPattern("Sub-category Heading 3 Menu item 3 of *");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeechPattern("Second sub-category Heading 3 Menu item 4 of *");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_UP); });
  sm()->ExpectSpeechPattern("Sub-category Heading 3 Menu item 3 of *");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_SPACE); });
  sm()->ExpectSpeech("Sub-category");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Sub-category");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Text");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Second sub-category");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Next page Button");

  sm()->Replay();
}

// Verifies that an announcement is triggered when focusing a blocked app
// ShelfItem.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       ShelfBlockedAppIconBadgeAnnouncement) {
  chromevox_test_utils()->EnableChromeVox();

  std::string app_id = "TestApp";

  // Set the app status as paused;
  apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kWeb, app_id);
  app->readiness = apps::Readiness::kDisabledByPolicy;
  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  apps::AppServiceProxyFactory::GetForProfile(GetProfile())
      ->OnApps(std::move(apps), apps::AppType::kWeb,
               false /* should_notify_initialized */);

  // Create and add a test app to the shelf model.
  ShelfItem item;
  item.id = ShelfID(app_id);
  item.title = u"TestAppTitle";
  item.type = ShelfItemType::TYPE_APP;
  item.app_status = AppStatus::kBlocked;
  ShelfModel::Get()->Add(item,
                         std::make_unique<TestShelfItemDelegate>(item.id));

  // Focus on the shelf.
  sm()->Call(
      [this]() { PerformAcceleratorAction(AcceleratorAction::kFocusShelf); });
  sm()->ExpectSpeech("Launcher");
  sm()->ExpectSpeech("Button");
  sm()->ExpectSpeech("Shelf");
  sm()->ExpectSpeech("Tool bar");

  // Press right key twice to focus the test app.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("TestAppTitle");
  sm()->ExpectSpeech("Button");

  // Check that when a blocked shelf app shelf item is focused, the correct
  // announcement occurs.
  sm()->ExpectSpeech("Blocked");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, NavigateChromeVoxMenu) {
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_PERIOD); });
  sm()->ExpectSpeech("Search the menus");
  sm()->Call([this]() {
    SendKeyPress(ui::VKEY_RIGHT);
    SendKeyPress(ui::VKEY_RIGHT);
    SendKeyPress(ui::VKEY_RIGHT);
  });
  sm()->ExpectSpeech("ChromeVox Menu");
  sm()->Call([this]() {
    SendKeyPress(ui::VKEY_DOWN);
    SendKeyPress(ui::VKEY_DOWN);
  });
  sm()->ExpectSpeech("Open ChromeVox Tutorial");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_SPACE); });
  sm()->ExpectSpeech("ChromeVox tutorial");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, OpenStatusTray) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([this]() {
    EXPECT_TRUE(
        PerformAcceleratorAction(AcceleratorAction::kToggleSystemTrayBubble));
  });
  sm()->ExpectSpeech("Quick Settings");

  sm()->Replay();
}

// TODO(https://crbug.com/388867840): Re-enable this test on MSAN. Note that
// MAYBE_ doesn't work well with parameterized tests.
#if !defined(MEMORY_SANITIZER)
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, OpenSettingsFromPanel) {
  chromevox_test_utils()->EnableChromeVox();
  base::RunLoop waiter;
  AccessibilityManager::Get()->SetOpenSettingsSubpageObserverForTest(
      base::BindLambdaForTesting([&waiter]() { waiter.Quit(); }));

  // Find the settings button in the panel.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_PERIOD); });
  sm()->ExpectSpeech("Search the menus");
  sm()->Call([this]() {
    SendKeyPress(ui::VKEY_TAB);
    SendKeyPress(ui::VKEY_TAB);
  });
  sm()->ExpectSpeech("ChromeVox Menus collapse");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm()->ExpectSpeech("ChromeVox Options");
  // Activate the settings button.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_SPACE); });
  sm()->Replay();
  // We should have tried to open the settings subpage.
  waiter.Run();
}
#endif  // !defined(MEMORY_SANITIZER)

// Fails on ASAN. See http://crbug.com/776308. (Note MAYBE_ doesn't work well
// with parameterized tests).
#if !defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, NavigateSystemTray) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([this]() {
    (PerformAcceleratorAction(AcceleratorAction::kToggleSystemTrayBubble));
  });

  sm()->ExpectSpeech("Quick Settings");
  // Settings button.
  sm()->Call([this]() { SendKeyPressWithShift(ui::VKEY_TAB); });
  sm()->ExpectSpeech("Settings");
  sm()->ExpectSpeech("Button");

  // Battery indicator.
  sm()->Call([this]() { SendKeyPressWithShift(ui::VKEY_TAB); });
  sm()->ExpectSpeech("Battery");

  // Guest mode sign out button.
  if (GetParam().variant() == kTestAsGuestUser) {
    sm()->Call([this]() { SendKeyPressWithShift(ui::VKEY_TAB); });
    sm()->ExpectSpeech("Exit guest");
  }

    // Shutdown button.
    sm()->Call([this]() { SendKeyPressWithShift(ui::VKEY_TAB); });
    sm()->ExpectSpeech("Power menu");
    sm()->ExpectSpeech("Button");

    sm()->Replay();
}
#endif  // !defined(ADDRESS_SANITIZER)

// TODO: these brightness announcements are actually not made.
// https://crbug.com/1064788
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_ScreenBrightness) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([this]() {
    (PerformAcceleratorAction(AcceleratorAction::kBrightnessUp));
  });
  sm()->ExpectSpeechPattern("Brightness * percent");

  sm()->Call([this]() {
    (PerformAcceleratorAction(AcceleratorAction::kBrightnessDown));
  });
  sm()->ExpectSpeechPattern("Brightness * percent");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, VolumeSlider) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([this]() {
    // Volume slider does not fire valueChanged event on first key press because
    // it has no widget.
    PerformAcceleratorAction(AcceleratorAction::kVolumeUp);
    PerformAcceleratorAction(AcceleratorAction::kVolumeUp);
  });
  sm()->ExpectSpeechPattern("* percent*");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, LandmarkNavigation) {
  ui::KeyboardCode semicolon = ui::VKEY_OEM_1;

  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
        <button autofocus>Start here</button>
        <p>before first landmark</p>
        <div role="application">application</div>
        <p>after application</p>
        <div role="banner">banner</div>
        <p>after banner</p>
        <div role="complementary">complementary</div>
        <p>after complementary</p>
        <form aria-label="form"></form>
        <button>after form</button>
        <div role="main">main</div>
        <h2>after main</h2>
        <nav>navigation</nav>
        <img alt="after navigation"></img>
        <input type="text" role="search" id="search"></input>
        <label for="search">search</label>
        <p>after search</p>)"));
  });
  sm()->ExpectSpeech("Start here");
  sm()->Call([this, semicolon]() { SendKeyPressWithSearch(semicolon); });
  sm()->ExpectSpeech("application");
  sm()->Call([this, semicolon]() { SendKeyPressWithSearch(semicolon); });
  sm()->ExpectSpeech("banner");
  sm()->Call([this, semicolon]() { SendKeyPressWithSearch(semicolon); });
  sm()->ExpectSpeech("complementary");
  sm()->Call([this, semicolon]() { SendKeyPressWithSearch(semicolon); });
  sm()->ExpectSpeech("form");
  sm()->Call([this, semicolon]() { SendKeyPressWithSearch(semicolon); });
  sm()->ExpectSpeech("main");
  sm()->Call([this, semicolon]() { SendKeyPressWithSearch(semicolon); });
  sm()->ExpectSpeech("navigation");
  sm()->Call([this, semicolon]() { SendKeyPressWithSearch(semicolon); });
  sm()->ExpectSpeech("search");
  sm()->Call(
      [this, semicolon]() { SendKeyPressWithSearchAndShift(semicolon); });
  sm()->ExpectSpeech("navigation");
  sm()->Call(
      [this, semicolon]() { SendKeyPressWithSearchAndShift(semicolon); });
  sm()->ExpectSpeech("main");

  // Navigate the landmark list.
  sm()->Call(
      [this, semicolon]() { SendKeyPressWithSearchAndControl(semicolon); });
  sm()->ExpectSpeech("Landmark Menu");
  sm()->ExpectSpeech("Application Menu item 1 of 7");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Banner Menu item 2 of 7");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Complementary Menu item 3 of 7");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Form Menu item 4 of 7");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Main Menu item 5 of 7");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Navigation Menu item 6 of 7");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Search Menu item 7 of 7");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_UP); });
  sm()->ExpectSpeech("Navigation Menu item 6 of 7");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_SPACE); });
  sm()->ExpectSpeech("Navigation");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_UP); });
  sm()->ExpectSpeech("after main");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, OverviewMode) {
  chromevox_test_utils()->EnableChromeVox();
  StablizeChromeVoxState();

  sm()->Call([this]() {
    (PerformAcceleratorAction(AcceleratorAction::kToggleOverview));
  });

  sm()->ExpectSpeech(
      "Entered window overview mode. Swipe to navigate, or press tab if using "
      "a keyboard.");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm()->ExpectSpeechPattern(
      "*data:text slash html;charset equal utf-8, percent 0A less than "
      "button autofocus greater than Click me less than slash button greater "
      "than");
  sm()->ExpectSpeechPattern("Press Ctrl plus W to close.");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, NextGraphic) {
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
             <button autofocus>Start here</button>
             <p>before the image</p>
             <img src="cat.png" alt="A cat curled up on the couch">
             <p>between the images</p>
             <img src="dog.png" alt="A happy dog holding a stick in its mouth">
             <p>after the images</p>)"));
  });
  sm()->ExpectSpeech("Start here");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_G); });
  sm()->ExpectSpeech("A cat curled up on the couch");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_G); });
  sm()->ExpectSpeech("A happy dog holding a stick in its mouth");
  sm()->Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_G); });
  sm()->ExpectSpeech("A cat curled up on the couch");
  sm()->Replay();
}

// TODO(crbug.com/40831399): Re-enable this test
// Verify that enable chromeVox won't end overview.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       DISABLED_EnableChromeVoxOnOverviewMode) {
  StablizeChromeVoxState();

  sm()->Call([this]() {
    (PerformAcceleratorAction(AcceleratorAction::kToggleOverview));
  });

  chromevox_test_utils()->EnableChromeVox();
  // Wait for Chromevox to start while in Overview before `sm()->Call`, which
  // pushes a callback when the last expected speech was seen.
  sm()->ExpectSpeechPattern(", window");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm()->ExpectSpeechPattern(
      "Chrom* - data:text slash html;charset equal utf-8, less than button "
      "autofocus greater than Click me less than slash button greater than");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ChromeVoxFindInPage) {
  chromevox_test_utils()->EnableChromeVox();
  StablizeChromeVoxState();

  // Press Search+/ to enter ChromeVox's "find in page".
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_2); });
  sm()->ExpectSpeech("Find in page");
  sm()->ExpectSpeech("Search");
  sm()->ExpectSpeech(
      "Type to search the page. Press enter to jump to the result, up or down "
      "arrows to browse results, keep typing to change your search, or escape "
      "to cancel.");

  sm()->Replay();
}

// TODO(crbug.com/40748296) Re-enable test
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       DISABLED_ChromeVoxNavigateAndSelect) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
            <h1>Title</h1>
            <button autofocus>Click me</button>)"));
  });
  sm()->ExpectSpeech("Click me");

  // Press Search+Left to navigate to the previous item.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_LEFT); });
  sm()->ExpectSpeech("Title");
  sm()->ExpectSpeech("Heading 1");

  // Press Search+S to select the text.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_S); });
  sm()->ExpectSpeech("Title");
  sm()->ExpectSpeech("selected");

  // Press again to end the selection.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_S); });
  sm()->ExpectSpeech("End selection");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ChromeVoxStickyMode) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
            <button autofocus>Click me</button>)"));
  });
  sm()->ExpectSpeech("Click me");

  // Press the sticky-key sequence: Search Search.
  sm()->Call([this]() { SendStickyKeyCommand(); });

  sm()->ExpectSpeech("Sticky mode enabled");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_H); });
  sm()->ExpectSpeech("No next heading");

  sm()->Call([this]() { SendStickyKeyCommand(); });
  sm()->ExpectSpeech("Sticky mode disabled");

  sm()->Replay();
}

// This tests ChromeVox sticky mode using raw key events as opposed to directly
// sending js commands above. This variant may be subject to flakes as it
// depends on more of the UI events stack and sticky mode invocation has a
// timing element to it.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ChromeVoxStickyModeRawKeys) {
  chromevox_test_utils()->EnableChromeVox();
  StablizeChromeVoxState();

  sm()->Call([this]() {
    SendKeyPress(ui::VKEY_LWIN);
    SendKeyPress(ui::VKEY_LWIN);
  });
  sm()->ExpectSpeech("Sticky mode enabled");

  sm()->Call([this]() {
    SendKeyPress(ui::VKEY_LWIN);
    SendKeyPress(ui::VKEY_LWIN);
  });
  sm()->ExpectSpeech("Sticky mode disabled");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       ChromeVoxStickyModeNavigatesAcrossMath) {
  chromevox_test_utils()->EnableChromeVox();
  NavigateToMathPage();

  // Press the sticky-key sequence: Search Search.
  sm()->Call([this]() { SendStickyKeyCommand(); });
  sm()->ExpectSpeech("Sticky mode enabled");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("StartFraction d y Over d x EndFraction equal");
  sm()->ExpectSpeech("Press up, down, left, or right to explore math");

  // Pressing right again does not enter the math exploration, since Sticky Mode
  // is on.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("end");

  sm()->Call([this]() { SendStickyKeyCommand(); });
  sm()->ExpectSpeech("Sticky mode disabled");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, NavigatesAcrossMath) {
  chromevox_test_utils()->EnableChromeVox();
  NavigateToMathPage();

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("StartFraction d y Over d x EndFraction equal");
  sm()->ExpectSpeech("Press up, down, left, or right to explore math");

  // We can leave the math node by navigating as usual.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("end");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, NavigatesWithinMath) {
  chromevox_test_utils()->EnableChromeVox();
  NavigateToMathPage();

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("StartFraction d y Over d x EndFraction equal");
  sm()->ExpectSpeech("Press up, down, left, or right to explore math");

  // Pressing down by itself leaves us within the Math exploration mode.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("StartFraction d y Over d x EndFraction");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("d y");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("D");

  // We can leave the math node by navigating as usual.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("end");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, TabNavigatesWhileInMath) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
        <button autofocus>start</button>
        <math>
          <mfrac>
            <mrow>
              <mi>d</mi>
              <mi>y</mi>
            </mrow>
            <mrow>
              <mi>d</mi>
              <mi>x</mi>
            </mrow>
          </mfrac>
          <mo>=</mo>
        </math>
        <p>not me</p>
        <button>sandwich</button>
        <p>end</p>)"));
  });
  sm()->ExpectSpeech("start");

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("StartFraction d y Over d x EndFraction equal");
  sm()->ExpectSpeech("Press up, down, left, or right to explore math");

  // Pressing down by itself leaves us within the Math exploration mode.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("StartFraction d y Over d x EndFraction");

  // Press tab. Focus should leave the math element and go to the next element
  // with keyboard focus.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm()->ExpectSpeech("sandwich");

  // Using the arrow keys does nothing as we are no longer focused on math.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectNextSpeechIsNot("d y");
  sm()->ExpectSpeech("end");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       DisablesStickyModeAndNavigatesWithinMath) {
  chromevox_test_utils()->EnableChromeVox();
  NavigateToMathPage();

  sm()->Call([this]() { SendStickyKeyCommand(); });
  sm()->ExpectSpeech("Sticky mode enabled");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("StartFraction d y Over d x EndFraction equal");
  sm()->ExpectSpeech("Press up, down, left, or right to explore math");

  sm()->Call([this]() { SendStickyKeyCommand(); });
  sm()->ExpectSpeech("Sticky mode disabled");

  // Pressing down by itself leaves us within the Math exploration mode.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("StartFraction d y Over d x EndFraction");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("d y");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("D");

  // We can leave the math node by navigating as usual.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("end");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       EnablesStickyModeAndNavigatesOutOfMath) {
  chromevox_test_utils()->EnableChromeVox();
  NavigateToMathPage();

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("StartFraction d y Over d x EndFraction equal");
  sm()->ExpectSpeech("Press up, down, left, or right to explore math");

  // Pressing down by itself leaves us within the Math exploration mode.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("StartFraction d y Over d x EndFraction");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("d y");

  // Turn on sticky mode.
  sm()->Call([this]() { SendStickyKeyCommand(); });
  sm()->ExpectSpeech("Sticky mode enabled");

  // We can leave the math node by navigating with the down arrow, rather than
  // diving down to the "D" term.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("end");

  sm()->Call([this]() { SendStickyKeyCommand(); });
  sm()->ExpectSpeech("Sticky mode disabled");

  sm()->Replay();
}

// TODO(crbug.com/41337748): Test is flaky.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_TouchExploreStatusTray) {
  chromevox_test_utils()->EnableChromeVox();

  base::SimpleTestTickClock clock;
  auto* clock_ptr = &clock;
  ui::SetEventTickClockForTesting(clock_ptr);

  auto* root_window = Shell::Get()->GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root_window);
  auto* generator_ptr = &generator;

  // Touch the status tray.
  sm()->Call([clock_ptr, generator_ptr]() {
    const gfx::Point& tray_center = Shell::Get()
                                        ->GetPrimaryRootWindowController()
                                        ->GetStatusAreaWidget()
                                        ->unified_system_tray()
                                        ->GetBoundsInScreen()
                                        .CenterPoint();

    ui::TouchEvent touch_press(
        ui::EventType::kTouchPressed, tray_center, base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_press);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move(
        ui::EventType::kTouchMoved, tray_center, base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move);
  });

  sm()->ExpectSpeechPattern("Status tray, time* Battery at* percent*");
  sm()->ExpectSpeech("Button");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ShowLinksList) {
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
        <button autofocus>Start here</button>
        <a href="https://google.com/">Google Search Engine</a>
        <a href="https://docs.google.com/">Google Docs</a>
        <a href="https://mail.google.com/">Gmail</a>)"));
  });
  sm()->ExpectSpeech("Start here");
  sm()->Call([this]() { SendKeyPressWithSearchAndControl(ui::VKEY_L); });
  sm()->ExpectSpeech("Link Menu");
  sm()->ExpectSpeech("Google Search Engine");
  sm()->ExpectSpeech("1 of 3");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Google Docs");
  sm()->ExpectSpeech("2 of 3");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Gmail");
  sm()->ExpectSpeech("3 of 3");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_UP); });
  sm()->ExpectSpeech("Google Docs");
  sm()->ExpectSpeech("2 of 3");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       TouchExploreRightEdgeVolumeSliderOn) {
  chromevox_test_utils()->EnableChromeVox();

  base::SimpleTestTickClock clock;
  auto* clock_ptr = &clock;
  ui::SetEventTickClockForTesting(clock_ptr);

  auto* root_window = Shell::Get()->GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root_window);
  auto* generator_ptr = &generator;

  // Force right edge volume slider gesture on.
  sm()->Call([] {
    AccessibilityController::Get()->EnableChromeVoxVolumeSlideGesture();
  });

  // Touch and slide on the right edge of the screen.
  sm()->Call([clock_ptr, generator_ptr]() {
    ui::TouchEvent touch_press(
        ui::EventType::kTouchPressed, gfx::Point(1280, 200),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_press);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move(
        ui::EventType::kTouchMoved, gfx::Point(1280, 300),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move2(
        ui::EventType::kTouchMoved, gfx::Point(1280, 400),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move2);
  });

  sm()->ExpectSpeech("Volume");
  sm()->ExpectSpeech("Slider");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       TouchExploreRightEdgeVolumeSliderOff) {
  chromevox_test_utils()->EnableChromeVox();

  base::SimpleTestTickClock clock;
  auto* clock_ptr = &clock;
  ui::SetEventTickClockForTesting(clock_ptr);

  auto* root_window = Shell::Get()->GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root_window);
  auto* generator_ptr = &generator;

  // Build a simple window with a button and position it at the right edge of
  // the screen.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);

  // Assert the right edge fits the below window.
  ASSERT_GE(root_window->bounds().width(), 1280);
  ASSERT_GE(root_window->bounds().height(), 800);

  // This is the right edge of the screen.
  params.bounds = {1050, 0, 50, 700};
  widget->Init(std::move(params));

  views::View* view = new views::View();
  view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  view->GetViewAccessibility().SetName(u"hello");
  view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  widget->GetRootView()->AddChildViewRaw(view);

  // Show the widget, then touch and slide on the right edge of the screen.
  sm()->Call([widget, clock_ptr, generator_ptr]() {
    widget->Show();
    ui::TouchEvent touch_press(
        ui::EventType::kTouchPressed, gfx::Point(1080, 200),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_press);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move(
        ui::EventType::kTouchMoved, gfx::Point(1080, 300),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move2(
        ui::EventType::kTouchMoved, gfx::Point(1080, 400),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move2);
  });

  // This should trigger reading of the button.
  sm()->ExpectSpeech("hello");
  sm()->ExpectSpeech("Button");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, TouchExploreSecondaryDisplay) {
  std::vector<RootWindowController*> root_controllers =
      Shell::GetAllRootWindowControllers();
  EXPECT_EQ(1U, root_controllers.size());

  // Make two displays, each 800 by 700, side by side.
  ShellTestApi shell_test_api;
  display::test::DisplayManagerTestApi(shell_test_api.display_manager())
      .UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, shell_test_api.display_manager()->GetNumDisplays());
  display::test::DisplayManagerTestApi display_manager_test_api(
      shell_test_api.display_manager());

  display::Screen* screen = display::Screen::Get();
  int64_t display2 = display_manager_test_api.GetSecondaryDisplay().id();
  screen->SetDisplayForNewWindows(display2);

  root_controllers = Shell::GetAllRootWindowControllers();
  EXPECT_EQ(2U, root_controllers.size());

  chromevox_test_utils()->EnableChromeVox();

  base::SimpleTestTickClock clock;
  auto* clock_ptr = &clock;
  ui::SetEventTickClockForTesting(clock_ptr);

  // Generate events to the secondary window which is at (800, 0).
  auto* root_window = root_controllers[1]->GetRootWindow();
  ui::test::EventGenerator generator(root_window);
  auto* generator_ptr = &generator;

  // Build a simple window with a button and position it at the right edge of
  // the screen.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.parent = root_window;

  // This is the right edge of the screen.
  params.bounds = {1550, 0, 50, 600};
  widget->Init(std::move(params));

  views::View* view = new views::View();
  view->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  view->GetViewAccessibility().SetName(u"hello");
  view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  widget->GetRootView()->AddChildViewRaw(view);

  // Show the widget, then touch and slide on the right edge of the screen.
  sm()->Call([widget, clock_ptr, generator_ptr]() {
    widget->Show();

    ui::TouchEvent touch_press(
        ui::EventType::kTouchPressed, gfx::Point(1580, 200),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_press);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move(
        ui::EventType::kTouchMoved, gfx::Point(1580, 300),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move2(
        ui::EventType::kTouchMoved, gfx::Point(1580, 400),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move2);
  });

  // This should trigger reading of the button.
  sm()->ExpectSpeech("hello");
  sm()->ExpectSpeech("Button");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, TouchExploreWebContents) {
  chromevox_test_utils()->EnableChromeVox();

  AutomationTestUtils test_utils(extension_misc::kChromeVoxExtensionId);
  sm()->Call([&test_utils]() { test_utils.SetUpTestSupport(); });

  base::SimpleTestTickClock clock;
  auto* clock_ptr = &clock;
  ui::SetEventTickClockForTesting(clock_ptr);

  auto* root_window = Shell::Get()->GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root_window);
  auto* generator_ptr = &generator;

  gfx::Rect b2_bounds;
  gfx::Rect b3_bounds;

  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
            <button id="b1" autofocus>First</button>
            <button id="b2">Second</button>
            <button id="b3">Third</button>
        )"));
  });
  sm()->ExpectSpeech("First");
  sm()->Call([clock_ptr, generator_ptr, &b2_bounds, &b3_bounds, &test_utils]() {
    b2_bounds = test_utils.GetNodeBoundsInRoot("Second", "button");
    b3_bounds = test_utils.GetNodeBoundsInRoot("Third", "button");

    ui::TouchEvent touch_press(
        ui::EventType::kTouchPressed, b2_bounds.top_center(),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_press);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move(
        ui::EventType::kTouchMoved, b2_bounds.CenterPoint(),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move2(
        ui::EventType::kTouchMoved, b2_bounds.left_center(),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move2);
  });
  sm()->ExpectSpeech("Second");
  sm()->Call([clock_ptr, generator_ptr, &b3_bounds]() {
    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move(
        ui::EventType::kTouchMoved, b3_bounds.right_center(),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move2(
        ui::EventType::kTouchMoved, b3_bounds.CenterPoint(),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move2);
  });
  sm()->ExpectSpeech("Third");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, TouchExploreWebContentsHighDPI) {
  ShellTestApi shell_test_api;
  // Use DPI of Strongbad, to reproduce b/295325508.
  display::test::DisplayManagerTestApi(shell_test_api.display_manager())
      .UpdateDisplay("800x700*1.77778");

  chromevox_test_utils()->EnableChromeVox();
  AutomationTestUtils test_utils(extension_misc::kChromeVoxExtensionId);
  sm()->Call([&test_utils]() { test_utils.SetUpTestSupport(); });

  base::SimpleTestTickClock clock;
  auto* clock_ptr = &clock;
  ui::SetEventTickClockForTesting(clock_ptr);

  auto* root_window = Shell::Get()->GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root_window);
  auto* generator_ptr = &generator;

  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
            <button id="b1" autofocus>First</button>
            <button id="b2">Second</button>
        )"));
  });
  sm()->ExpectSpeech("First");
  sm()->Call([clock_ptr, generator_ptr, &test_utils]() {
    float scale_factor = 1.77778;
    gfx::Rect b2_bounds = test_utils.GetNodeBoundsInRoot("Second", "button");
    // GetNodeBoundsInRoot returns in DIPs. Multiply by resolution to get px,
    // which is where we need to touch on a high density screen.
    b2_bounds.set_x(b2_bounds.x() * scale_factor);
    b2_bounds.set_y(b2_bounds.y() * scale_factor);
    b2_bounds.set_width(b2_bounds.width() * scale_factor);
    b2_bounds.set_height(b2_bounds.height() * scale_factor);

    ui::TouchEvent touch_press(
        ui::EventType::kTouchPressed, b2_bounds.bottom_center(),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_press);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move(
        ui::EventType::kTouchMoved, b2_bounds.CenterPoint(),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move2(
        ui::EventType::kTouchMoved, b2_bounds.right_center(),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move2);
  });
  sm()->ExpectSpeech("Second");
  sm()->Replay();
}

// TODO(b/287488905): Add test for touch explore with screen magnifier and high
// DPI.

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ChromeVoxNextTabRecovery) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
            <button id='b1' autofocus>11</button>
            <button>22</button>
            <button>33</button>
            <h1>Middle</h1>
            <button>44</button>
            <button>55</button>
            <div id=console aria-live=polite></div>
            <script>
              var b1 = document.getElementById('b1');
              b1.addEventListener('blur', function() {
                document.getElementById('console').innerText =
                    'button lost focus';
              });
            </script>)"));
  });
  sm()->ExpectSpeech("Button");

  // Press Search+H to go to the next heading
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_H); });

  sm()->ExpectSpeech("Middle");

  // To ensure that the setSequentialFocusNavigationStartingPoint has
  // executed before pressing Tab, the page has an event handler waiting
  // for the 'blur' event on the button, and when it loses focus it
  // triggers a live region announcement that we wait for, here.
  sm()->ExpectSpeech("button lost focus");

  // Now we know that focus has left the button, so the sequential focus
  // navigation starting point must be on the heading. Press Tab and
  // ensure that we land on the first link past the heading.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_TAB); });
  sm()->ExpectSpeech("44");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest,
                       MoveByCharacterPhoneticSpeechAndHints) {
  chromevox_test_utils()->EnableChromeVox();
  StablizeChromeVoxState();
  sm()->ExpectSpeech("Button");
  sm()->ExpectSpeech("Press Search plus Space to activate");

  // Move by character through the button.
  // Assert that phonetic speech and hints are delayed.
  sm()->Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("L");
  sm()->ExpectSpeech("lima");
  sm()->Call([this]() {
    EXPECT_GE(sm()->GetDelayForLastUtterance(),
              kExpectedPhoneticSpeechAndHintDelay);
  });
  sm()->ExpectSpeech("Press Search plus Space to activate");
  sm()->Call([this]() {
    EXPECT_GE(sm()->GetDelayForLastUtterance(),
              kExpectedPhoneticSpeechAndHintDelay);
  });
  sm()->Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("I");
  sm()->ExpectSpeech("india");
  sm()->Call([this]() {
    EXPECT_GE(sm()->GetDelayForLastUtterance(),
              kExpectedPhoneticSpeechAndHintDelay);
  });
  sm()->ExpectSpeech("Press Search plus Space to activate");
  sm()->Call([this]() {
    EXPECT_GE(sm()->GetDelayForLastUtterance(),
              kExpectedPhoneticSpeechAndHintDelay);
  });
  sm()->Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("C");
  sm()->ExpectSpeech("charlie");
  sm()->Call([this]() {
    EXPECT_GE(sm()->GetDelayForLastUtterance(),
              kExpectedPhoneticSpeechAndHintDelay);
  });
  sm()->ExpectSpeech("Press Search plus Space to activate");
  sm()->Call([this]() {
    EXPECT_GE(sm()->GetDelayForLastUtterance(),
              kExpectedPhoneticSpeechAndHintDelay);
  });
  sm()->Call([this]() { SendKeyPressWithSearchAndShift(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("K");
  sm()->ExpectSpeech("kilo");
  sm()->Call([this]() {
    EXPECT_GE(sm()->GetDelayForLastUtterance(),
              kExpectedPhoneticSpeechAndHintDelay);
  });
  sm()->ExpectSpeech("Press Search plus Space to activate");
  sm()->Call([this]() {
    EXPECT_GE(sm()->GetDelayForLastUtterance(),
              kExpectedPhoneticSpeechAndHintDelay);
  });
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ResetTtsSettings) {
  chromevox_test_utils()->EnableChromeVox();
  StablizeChromeVoxState();

  // Reset Tts settings using hotkey and assert speech output.
  sm()->Call(
      [this]() { SendKeyPressWithSearchAndControlAndShift(ui::VKEY_OEM_5); });
  sm()->ExpectSpeech("Reset text to speech settings to default values");
  // Increase speech rate.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_4); });
  sm()->ExpectSpeech("Rate 19 percent");
  // Increase speech pitch.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_6); });
  sm()->ExpectSpeech("Pitch 50 percent");
  // Reset Tts settings again.
  sm()->Call(
      [this]() { SendKeyPressWithSearchAndControlAndShift(ui::VKEY_OEM_5); });
  sm()->ExpectSpeech("Reset text to speech settings to default values");
  // Ensure that increasing speech rate and pitch jump to the same values as
  // before.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_4); });
  sm()->ExpectSpeech("Rate 19 percent");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_OEM_6); });
  sm()->ExpectSpeech("Pitch 50 percent");
  sm()->Replay();
}

// Tests the keyboard shortcut to cycle the punctuation echo setting,
// Search+A then P.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, TogglePunctuationEcho) {
  chromevox_test_utils()->EnableChromeVox();
  StablizeChromeVoxState();
  sm()->Call([this]() {
    SendKeyPressWithSearch(ui::VKEY_A);
    SendKeyPress(ui::VKEY_P);
  });
  sm()->ExpectSpeech("All punctuation");
  sm()->Call([this]() {
    SendKeyPressWithSearch(ui::VKEY_A);
    SendKeyPress(ui::VKEY_P);
  });
  sm()->ExpectSpeech("No punctuation");
  sm()->Call([this]() {
    SendKeyPressWithSearch(ui::VKEY_A);
    SendKeyPress(ui::VKEY_P);
  });
  sm()->ExpectSpeech("Some punctuation");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ShowFormControlsList) {
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
            <button autofocus>Start here</button>
            <input type="text" id="text"></input>
            <label for="text">Name</label>
            <p>Other text</p>
            <button>Make it shiny</button>
            <input type="checkbox" id="checkbox"></input>
            <label for="checkbox">Express delivery</label>
            <input type="range" id="slider"></input>
            <label for="slider">Percent cotton</label>)"));
  });
  sm()->ExpectSpeech("Start here");
  sm()->Call([this]() { SendKeyPressWithSearchAndControl(ui::VKEY_F); });
  sm()->ExpectSpeech("Form Controls Menu");
  sm()->ExpectSpeech("Start here Button");
  sm()->ExpectSpeech("Menu item 1 of ");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Name Edit text");
  sm()->ExpectSpeech("Menu item 2 of ");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Make it shiny Button");
  sm()->ExpectSpeech("Menu item 3 of ");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Express delivery Check box");
  sm()->ExpectSpeech("Menu item 4 of ");
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Percent cotton Slider");
  sm()->ExpectSpeech("Menu item 5 of ");

  sm()->Replay();
}

// TODO(crbug.com/1310316): Test is flaky.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_SmartStickyMode) {
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
        <p>start</p>
        <input autofocus type='text'>
        <p>end</p>)"));
  });

  // The input is autofocused.
  sm()->ExpectSpeech("Edit text");

  // First, navigate with sticky mode on.
  sm()->Call([this]() { SendStickyKeyCommand(); });
  sm()->ExpectSpeech("Sticky mode enabled");

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->ExpectNextSpeechIsNotPattern("Sticky mode *abled");
  sm()->ExpectSpeech("end");

  // Jump to beginning.
  sm()->Call([this]() { SendKeyPressWithSearchAndControl(ui::VKEY_LEFT); });
  sm()->ExpectNextSpeechIsNotPattern("Sticky mode *abled");
  sm()->ExpectSpeech("start");

  // The nextEditText command is explicitly excluded from toggling.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_E); });
  sm()->ExpectNextSpeechIsNotPattern("Sticky mode *abled");
  sm()->ExpectSpeech("Edit text");

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->ExpectNextSpeechIsNotPattern("Sticky mode *abled");
  sm()->ExpectSpeech("end");

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_LEFT); });
  sm()->ExpectSpeech("Sticky mode disabled");
  sm()->ExpectSpeech("Edit text");

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_LEFT); });
  sm()->ExpectSpeech("Sticky mode enabled");
  sm()->ExpectSpeech("start");

  // Try a few jump commands and linear nav with no Search modifier. We never
  // leave sticky mode.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_E); });
  sm()->ExpectSpeech("Edit text");

  sm()->Call([this]() { SendKeyPressWithShift(ui::VKEY_F); });
  sm()->ExpectSpeech("Edit text");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("end");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_F); });
  sm()->ExpectSpeech("Edit text");

  sm()->Call([this]() { SendKeyPressWithShift(ui::VKEY_E); });
  sm()->ExpectSpeech("Edit text");

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_LEFT); });
  sm()->ExpectSpeech("start");

  // Now, navigate with sticky mode off.
  sm()->Call([this]() { SendStickyKeyCommand(); });
  sm()->ExpectSpeech("Sticky mode disabled");

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->ExpectNextSpeechIsNotPattern("Sticky mode *abled");
  sm()->ExpectSpeech("Edit text");

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->ExpectNextSpeechIsNotPattern("Sticky mode *abled");
  sm()->ExpectSpeech("end");

  sm()->Replay();
}

class TestBacklightsObserver : public ScreenBacklightObserver {
 public:
  explicit TestBacklightsObserver(
      BacklightsForcedOffSetter* backlights_setter) {
    backlights_forced_off_ = backlights_setter->backlights_forced_off();
    scoped_observation_.Observe(backlights_setter);
  }
  ~TestBacklightsObserver() override = default;
  TestBacklightsObserver(const TestBacklightsObserver&) = delete;
  TestBacklightsObserver& operator=(const TestBacklightsObserver&) = delete;

  // ScreenBacklightObserver:
  void OnBacklightsForcedOffChanged(bool backlights_forced_off) override {
    if (backlights_forced_off_ == backlights_forced_off) {
      return;
    }

    backlights_forced_off_ = backlights_forced_off;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void WaitForBacklightStateChange() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  bool backlights_forced_off() const { return backlights_forced_off_; }

 private:
  bool backlights_forced_off_;
  std::unique_ptr<base::RunLoop> run_loop_;

  base::ScopedObservation<BacklightsForcedOffSetter, ScreenBacklightObserver>
      scoped_observation_{this};
};

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DarkenScreenConfirmation) {
  chromevox_test_utils()->EnableChromeVox();
  StablizeChromeVoxState();
  EXPECT_FALSE(
      Shell::Get()->backlights_forced_off_setter()->backlights_forced_off());
  BacklightsForcedOffSetter* backlights_setter =
      Shell::Get()->backlights_forced_off_setter();
  TestBacklightsObserver observer(backlights_setter);

  // Try to darken screen and check the dialog is shown.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_F7); });
  sm()->ExpectSpeech("Turn off screen?");
  sm()->ExpectSpeech("Dialog");
  // TODO(crbug.com/40777708) - Improve the generation of summaries across
  // ChromeOS. Expect the content to be spoken once it has been improved.
  /*sm()->ExpectSpeech(
      "Turn off screen? This improves privacy by turning off your screen so it "
      "isn’t visible to others. You can always turn the screen back on by "
      "pressing Search plus Brightness up. Cancel Continue");*/
  sm()->ExpectSpeech("Continue");
  sm()->ExpectSpeech("default");
  sm()->ExpectSpeech("Button");

  sm()->Call([]() {
    // Accept the dialog and see that the screen is darkened.
    AccessibilityConfirmationDialog* dialog_ =
        Shell::Get()
            ->accessibility_controller()
            ->GetConfirmationDialogForTest();
    ASSERT_TRUE(dialog_ != nullptr);
    dialog_->Accept();
  });
  sm()->ExpectSpeech("Screen off");
  // Make sure Ash gets the backlight change request.
  sm()->Call([&observer = observer, backlights_setter = backlights_setter]() {
    if (observer.backlights_forced_off()) {
      return;
    }
    observer.WaitForBacklightStateChange();
    EXPECT_TRUE(backlights_setter->backlights_forced_off());
  });

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_F7); });
  sm()->ExpectNextSpeechIsNot("Continue");
  sm()->ExpectSpeech("Screen on");
  sm()->Call([&observer = observer, backlights_setter = backlights_setter]() {
    if (!observer.backlights_forced_off()) {
      return;
    }
    observer.WaitForBacklightStateChange();
    EXPECT_FALSE(backlights_setter->backlights_forced_off());
  });

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_F7); });
  sm()->ExpectNextSpeechIsNot("Continue");
  sm()->ExpectSpeech("Screen off");
  sm()->Call([&observer = observer, backlights_setter = backlights_setter]() {
    if (observer.backlights_forced_off()) {
      return;
    }
    observer.WaitForBacklightStateChange();
    EXPECT_TRUE(backlights_setter->backlights_forced_off());
  });

  sm()->Replay();
}

// Tests basic behavior of the tutorial when signed in.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, Tutorial) {
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
        <button autofocus>Testing</button>)"));
  });
  sm()->ExpectSpeech("Testing");
  sm()->Call([this]() {
    SendKeyPressWithSearch(ui::VKEY_O);
    SendKeyPress(ui::VKEY_T);
  });
  sm()->ExpectSpeech("ChromeVox tutorial");
  sm()->ExpectSpeech(
      "Press Search plus Right Arrow, or Search plus Left Arrow to browse "
      "topics");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("Quick orientation");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("Essential keys");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_B); });
  sm()->ExpectSpeech("Exit tutorial");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm()->ExpectSpeech("Testing");
  sm()->Replay();
}

// TODO(crbug.com/40930988): Re-enable this test
IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, DISABLED_ClipboardCopySpeech) {
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call([this]() {
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
        <input autofocus type='text' value='Foo'></input>)"));
  });

  // The input is autofocused.
  sm()->ExpectSpeech("Edit text");

  // Select and copy the first character.
  sm()->Call([this]() {
    SendKeyPressWithShift(ui::VKEY_RIGHT);
    SendKeyPressWithControl(ui::VKEY_C);
  });
  sm()->ExpectSpeech("copy F.");

  // Select and copy the first two characters.
  sm()->Call([this]() {
    SendKeyPressWithShift(ui::VKEY_RIGHT);
    SendKeyPressWithControl(ui::VKEY_C);
  });
  sm()->ExpectSpeech("copy Fo.");

  // Select and copy all characters.
  sm()->Call([this]() {
    SendKeyPressWithShift(ui::VKEY_RIGHT);
    SendKeyPressWithControl(ui::VKEY_C);
  });
  sm()->ExpectSpeech("copy Foo.");

  // Do it again with the command Search+Ctrl+C, which should do the same thing
  // but triggered through ChromeVox via synthesized keys.
  sm()->Call([this]() { SendKeyPressWithSearchAndControl(ui::VKEY_C); });
  sm()->ExpectSpeech("copy Foo.");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, OrientationChanged) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([]() {
    Shell::Get()->display_configuration_controller()->SetDisplayRotation(
        ash::Shell::Get()->display_manager()->GetDisplayAt(0).id(),
        display::Display::ROTATE_90, display::Display::RotationSource::USER);
  });

  sm()->ExpectSpeech("portrait");

  sm()->Call([]() {
    Shell::Get()->display_configuration_controller()->SetDisplayRotation(
        ash::Shell::Get()->display_manager()->GetDisplayAt(0).id(),
        display::Display::ROTATE_180, display::Display::RotationSource::USER);
  });

  sm()->ExpectSpeech("landscape");

  sm()->Call([]() {
    Shell::Get()->display_configuration_controller()->SetDisplayRotation(
        ash::Shell::Get()->display_manager()->GetDisplayAt(0).id(),
        display::Display::ROTATE_270, display::Display::RotationSource::USER);
  });

  sm()->ExpectSpeech("portrait");

  sm()->ExpectHadNoRepeatedSpeech();

  sm()->Replay();
}

// Spoken feedback tests of the out-of-box experience.
class OobeSpokenFeedbackTest : public OobeBaseTest {
 public:
  OobeSpokenFeedbackTest() = default;
  OobeSpokenFeedbackTest(const OobeSpokenFeedbackTest&) = delete;
  OobeSpokenFeedbackTest& operator=(const OobeSpokenFeedbackTest&) = delete;
  ~OobeSpokenFeedbackTest() override = default;

 protected:
  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    // Many bots don't have keyboard/mice which triggers the HID detection
    // dialog in the OOBE.  Avoid confusing the tests with that.
    command_line->AppendSwitch(switches::kDisableHIDDetectionOnOOBEForTesting);
    // We only start the tutorial in OOBE if the device is a Chromebook, so set
    // the device type so tutorial-related behavior can be tested.
    command_line->AppendSwitchASCII(switches::kFormFactor, "CHROMEBOOK");
  }
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        Shell::Get()->GetPrimaryRootWindow());
    chromevox_test_utils_ = std::make_unique<ChromeVoxTestUtils>();
  }
  void TearDownOnMainThread() override {
    event_generator_.reset();
    chromevox_test_utils_.reset();
    OobeBaseTest::TearDownOnMainThread();
  }

  test::SpeechMonitor* sm() { return chromevox_test_utils_->sm(); }

  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  std::unique_ptr<ChromeVoxTestUtils> chromevox_test_utils_;
};

// TODO(crbug.com/1310682) - Re-enable this test.
IN_PROC_BROWSER_TEST_F(OobeSpokenFeedbackTest, DISABLED_SpokenFeedbackInOobe) {
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  AccessibilityManager::Get()->EnableSpokenFeedbackWithTutorial();

  // If ChromeVox is started in OOBE, the tutorial is automatically opened.
  sm()->ExpectSpeech("Welcome to ChromeVox!");

  // The tutorial can be exited by pressing Escape.
  sm()->Call(
      [&]() { event_generator_->PressAndReleaseKey(ui::VKEY_ESCAPE, 0); });

  sm()->ExpectSpeech("Get started");

  sm()->Call([&]() { event_generator_->PressAndReleaseKey(ui::VKEY_TAB, 0); });
  sm()->ExpectSpeech("Pause animation");

  sm()->Call([&]() { event_generator_->PressAndReleaseKey(ui::VKEY_TAB, 0); });
  sm()->ExpectSpeechPattern("*Status tray*");
  sm()->ExpectSpeech("Button");

  sm()->Replay();
}

// TODO(akihiroota): fix flakiness: http://crbug.com/1172390
IN_PROC_BROWSER_TEST_F(OobeSpokenFeedbackTest,
                       DISABLED_SpokenFeedbackTutorialInOobe) {
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  sm()->ExpectSpeech("Welcome to ChromeVox!");
  sm()->ExpectSpeechPattern(
      "Welcome to the ChromeVox tutorial*When you're ready, use the spacebar "
      "to move to the next lesson.");
  // Press space to move to the next lesson.
  sm()->Call(
      [&]() { event_generator_->PressAndReleaseKey(ui::VKEY_SPACE, 0); });
  sm()->ExpectSpeech("Essential Keys: Control");
  sm()->ExpectSpeechPattern("*To continue, press the Control key.*");
  // Press control to move to the next lesson.
  sm()->Call(
      [&]() { event_generator_->PressAndReleaseKey(ui::VKEY_CONTROL, 0); });
  sm()->ExpectSpeechPattern("*To continue, press the left Shift key.");
  sm()->Replay();
}

class SigninToUserProfileSwitchTest : public OobeSpokenFeedbackTest {
 public:
  // OobeSpokenFeedbackTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeSpokenFeedbackTest::SetUpCommandLine(command_line);
    // Force the help app to launch in the background.
    command_line->AppendSwitch(switches::kForceFirstRunUI);
  }

 protected:
  LoginManagerMixin login_manager_{&mixin_host_};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};
};

// Verifies that spoken feedback correctly handles profile switch (signin ->
// user) and announces the sync consent screen correctly.
// TODO(crbug.com/1184714): Fix flakiness.
IN_PROC_BROWSER_TEST_F(SigninToUserProfileSwitchTest, DISABLED_LoginAsNewUser) {
  // Force sync screen.
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  sm()->ExpectSpeechPattern("*");

  sm()->Call([this]() {
    ASSERT_TRUE(IsSigninBrowserContext(AccessibilityManager::Get()->profile()));
    login_manager_.LoginAsNewRegularUser();
  });

  sm()->ExpectSpeechPattern("Welcome to the ChromeVox tutorial*");

  // The tutorial can be exited by pressing Escape.
  sm()->Call(
      [&]() { event_generator_->PressAndReleaseKey(ui::VKEY_ESCAPE, 0); });

  sm()->ExpectSpeech("Accept and continue");

  // Check that profile switched to the active user.
  sm()->Call([&]() {
    ASSERT_EQ(AccessibilityManager::Get()->profile(),
              ProfileManager::GetActiveUserProfile());
  });
  sm()->Replay();
}

class ShortcutsAppSpokenFeedbackTest : public LoggedInSpokenFeedbackTest {
 public:
  ShortcutsAppSpokenFeedbackTest() = default;
  ShortcutsAppSpokenFeedbackTest(const ShortcutsAppSpokenFeedbackTest&) =
      delete;
  ShortcutsAppSpokenFeedbackTest& operator=(
      const ShortcutsAppSpokenFeedbackTest&) = delete;
  ~ShortcutsAppSpokenFeedbackTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    ManifestV2,
    ShortcutsAppSpokenFeedbackTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3,
    ShortcutsAppSpokenFeedbackTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree)));

// TODO(b/288602247): The test is flaky.
IN_PROC_BROWSER_TEST_P(ShortcutsAppSpokenFeedbackTest,
                       DISABLED_ShortcutCustomization) {
  chromevox_test_utils()->EnableChromeVox();
  sm()->Call(
      [this]() { NavigateToUrl(GURL("chrome://shortcut-customization")); });
  sm()->ExpectSpeech("Search shortcuts");

  // Move through all tabs; make a few expectations along the way.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("General");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Accessibility");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Keyboard settings");

  // Moving forward again should dive into the list of shortcuts for the
  // category.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("General controls");
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Open slash close Launcher");
  sm()->ExpectSpeech("row 1 column 1");
  sm()->Replay();
}

class SpokenFeedbackWithCandidateWindowTest
    : public LoggedInSpokenFeedbackTest {
 public:
  SpokenFeedbackWithCandidateWindowTest() = default;
  SpokenFeedbackWithCandidateWindowTest(
      const SpokenFeedbackWithCandidateWindowTest&) = delete;
  SpokenFeedbackWithCandidateWindowTest& operator=(
      const SpokenFeedbackWithCandidateWindowTest&) = delete;
  ~SpokenFeedbackWithCandidateWindowTest() override = default;

  void SetUpOnMainThread() override {
    LoggedInSpokenFeedbackTest::SetUpOnMainThread();

    aura::Window* parent =
        ash::Shell::GetContainer(Shell::Get()->GetPrimaryRootWindow(),
                                 ash::kShellWindowId_MenuContainer);

    candidate_window_view_ = new ui::ime::CandidateWindowView(parent);
    candidate_window_view_->InitWidget();
  }
  void TearDownOnMainThread() override {
    candidate_window_view_.ExtractAsDangling()->GetWidget()->CloseNow();
    LoggedInSpokenFeedbackTest::TearDownOnMainThread();
  }

  raw_ptr<ui::ime::CandidateWindowView> candidate_window_view_;
};

INSTANTIATE_TEST_SUITE_P(
    ManifestV2,
    SpokenFeedbackWithCandidateWindowTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3,
    SpokenFeedbackWithCandidateWindowTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree)));

IN_PROC_BROWSER_TEST_P(SpokenFeedbackWithCandidateWindowTest,
                       SpeakSelectedItem) {
  chromevox_test_utils()->EnableChromeVox();

  ui::CandidateWindow candidate_window;
  candidate_window.set_cursor_position(0);
  candidate_window.set_page_size(2);
  candidate_window.mutable_candidates()->clear();
  candidate_window.set_orientation(ui::CandidateWindow::VERTICAL);
  candidate_window.set_is_user_selecting(true);
  for (size_t i = 0; i < 2; ++i) {
    ui::CandidateWindow::Entry entry;
    entry.value = u"value " + base::NumberToString16(i);
    entry.label = u"label " + base::NumberToString16(i);
    candidate_window.mutable_candidates()->push_back(entry);
  }

  sm()->Call([this, &candidate_window]() {
    candidate_window_view_->GetWidget()->Show();
    candidate_window_view_->UpdateCandidates(candidate_window);
    candidate_window_view_->ShowLookupTable();
  });
  sm()->ExpectSpeech("value 0");

  // Move selection to another item.
  sm()->Call([this, &candidate_window]() {
    candidate_window.set_cursor_position(1);
    candidate_window_view_->UpdateCandidates(candidate_window);
  });
  sm()->ExpectSpeech("value 1");

  // Simulate pagination.
  sm()->Call([this, &candidate_window]() {
    candidate_window.set_cursor_position(0);
    candidate_window.mutable_candidates()->at(0).value = u"value 2";
    candidate_window.mutable_candidates()->at(0).label = u"label 2";
    candidate_window.mutable_candidates()->at(1).value = u"value 3";
    candidate_window.mutable_candidates()->at(1).label = u"label 3";
    candidate_window_view_->UpdateCandidates(candidate_window);
  });
  sm()->ExpectSpeech(test::SpeechMonitor::Expectation("value 2").WithoutText(
      {"value 0", "value 1", "value 3"}));

  sm()->Replay();
}

class SpokenFeedbackWithMagnifierTest : public SpokenFeedbackTest {
 protected:
  SpokenFeedbackWithMagnifierTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SpokenFeedbackTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityMagnifierFollowsChromeVox);
  }

  void SetUpOnMainThread() override {
    SpokenFeedbackTest::SetUpOnMainThread();

    EnableMagnifier();
    chromevox_test_utils()->EnableChromeVox();

    test_utils_ = std::make_unique<AutomationTestUtils>(
        extension_misc::kChromeVoxExtensionId);
  }

  void EnableMagnifier() {
    Profile* profile = AccessibilityManager::Get()->profile();
    extensions::ExtensionHostTestHelper host_helper(
        profile, extension_misc::kAccessibilityCommonExtensionId);
    profile->GetPrefs()->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled,
                                    true);

    FullscreenMagnifierController* fullscreen_magnifier_controller =
        Shell::Get()->fullscreen_magnifier_controller();
    MagnifierAnimationWaiter waiter(fullscreen_magnifier_controller);
    waiter.Wait();

    ASSERT_TRUE(MagnificationManager::Get()->IsMagnifierEnabled());
    host_helper.WaitForHostCompletedFirstLoad();
    FullscreenMagnifierTestHelper::WaitForMagnifierJSReady(profile);

    // Set Magnifier.IGNORE_AT_UPDATES_AFTER_OTHER_MOVE_MS to a small duration
    // to allow for testing with automation interactions, which move faster than
    // the ignore duration used in production.
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string script = base::StringPrintf(R"JS(
        (async function() {
          globalThis.accessibilityCommon.setFeatureLoadCallbackForTest(
              'magnifier', () => {
                globalThis.accessibilityCommon.magnifier_.setIgnoreAssistiveTechnologyUpdatesAfterOtherMoveDurationForTest(
                    200);
                chrome.test.sendScriptResult('ready');
              });
        })();
      )JS");
    base::Value result =
        extensions::browsertest_util::ExecuteScriptInBackgroundPage(
            profile, extension_misc::kAccessibilityCommonExtensionId, script);
    ASSERT_EQ("ready", result);
  }

  void WaitForMagnifierViewportOnBounds(gfx::Rect focus_bounds) {
    FullscreenMagnifierController* fullscreen_magnifier_controller =
        Shell::Get()->fullscreen_magnifier_controller();
    MagnifierAnimationWaiter waiter(fullscreen_magnifier_controller);

    // Magnifier should now move to the focused area.
    while (!fullscreen_magnifier_controller->GetViewportRect().Intersects(
        focus_bounds)) {
      waiter.Wait();
    }

    // Check that magnifier viewport is on node.
    gfx::Rect final_viewport =
        fullscreen_magnifier_controller->GetViewportRect();
    EXPECT_TRUE(final_viewport.Intersects(focus_bounds));
  }

  void EnsureMagnifierViewportNotOnBounds(gfx::Rect focus_bounds) {
    gfx::Rect current_viewport =
        Shell::Get()->fullscreen_magnifier_controller()->GetViewportRect();
    EXPECT_FALSE(current_viewport.IsEmpty());
    EXPECT_FALSE(focus_bounds.size().IsEmpty());

    // Ensure magnifier viewport is not currently already intersecting node.
    EXPECT_FALSE(current_viewport.Intersects(focus_bounds));
  }

  AutomationTestUtils* test_utils() { return test_utils_.get(); }

 private:
  std::unique_ptr<AutomationTestUtils> test_utils_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ManifestV2NormalUser,
    SpokenFeedbackWithMagnifierTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo,
                                               kTestAsNormalUser)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV2GuestUser,
    SpokenFeedbackWithMagnifierTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo,
                                               kTestAsGuestUser)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3NormalUser,
    SpokenFeedbackWithMagnifierTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree,
                                               kTestAsNormalUser)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3GuestUser,
    SpokenFeedbackWithMagnifierTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree,
                                               kTestAsGuestUser)));

IN_PROC_BROWSER_TEST_P(SpokenFeedbackWithMagnifierTest,
                       FullscreenMagnifierButton) {
  gfx::Rect focus_bounds;

  sm()->Call([this, &focus_bounds]() {
    test_utils()->SetUpTestSupport();

    // Load a page with interactive text node that would get keyboard
    // focus and should get magnifier focus.
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
        <button>Hello world</button>)"));

    // Set magnifier scale to something quite big so that the initial bounds
    // of the button are not within the magnifier bounds.
    AccessibilityManager::Get()->profile()->GetPrefs()->SetDouble(
        prefs::kAccessibilityScreenMagnifierScale, 4.0);

    focus_bounds = test_utils()->GetNodeBoundsInRoot("Hello world", "button");

    EnsureMagnifierViewportNotOnBounds(focus_bounds);

    // Press right key to focus the button.
    SendKeyPressWithSearch(ui::VKEY_RIGHT);
  });

  sm()->ExpectSpeech("Hello world");

  sm()->Call([this, &focus_bounds]() {
    WaitForMagnifierViewportOnBounds(focus_bounds);
  });

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackWithMagnifierTest,
                       FullscreenMagnifierStaticTextSingleLine) {
  gfx::Rect focus_bounds;

  sm()->Call([this, &focus_bounds]() {
    test_utils()->SetUpTestSupport();

    // Load a page with non-interactive text node that would not get keyboard
    // focus so would not already get magnifier focus.
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
        <p>Hello world</p>)"));

    // Set magnifier scale to something quite big so that the initial bounds
    // of the text are not within the magnifier bounds.
    AccessibilityManager::Get()->profile()->GetPrefs()->SetDouble(
        prefs::kAccessibilityScreenMagnifierScale, 4.0);

    focus_bounds =
        test_utils()->GetNodeBoundsInRoot("Hello world", "staticText");
    EnsureMagnifierViewportNotOnBounds(focus_bounds);

    // Press right key to focus the text node.
    SendKeyPressWithSearch(ui::VKEY_RIGHT);
  });

  sm()->ExpectSpeech("Hello world");

  sm()->Call([this, &focus_bounds]() {
    WaitForMagnifierViewportOnBounds(focus_bounds);
  });

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackWithMagnifierTest,
                       FullscreenMagnifierStaticTextMultipleLines) {
  gfx::Rect focus_bounds;

  sm()->Call([this, &focus_bounds]() {
    test_utils()->SetUpTestSupport();

    // Load a page with non-interactive text node that would not get
    // keyboard focus so would not already get magnifier focus.
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
        <p>Line 1</p>
        <p>Line 2</p>
        <p>Line 3</p>)"));

    // Set magnifier scale to something quite big so that the initial bounds
    // of the text are not within the magnifier bounds.
    AccessibilityManager::Get()->profile()->GetPrefs()->SetDouble(
        prefs::kAccessibilityScreenMagnifierScale, 8.0);

    // Verify first line.
    focus_bounds = test_utils()->GetNodeBoundsInRoot("Line 1", "staticText");
    EnsureMagnifierViewportNotOnBounds(focus_bounds);

    // Press right key to focus the text node.
    SendKeyPressWithSearch(ui::VKEY_RIGHT);
  });

  sm()->ExpectSpeech("Line 1");

  sm()->Call([this, &focus_bounds]() {
    WaitForMagnifierViewportOnBounds(focus_bounds);

    // Verify last line, which should not be currently intersecting the
    // viewport.
    focus_bounds = test_utils()->GetNodeBoundsInRoot("Line 3", "staticText");
    EnsureMagnifierViewportNotOnBounds(focus_bounds);

    // Press right key to focus the text node.
    SendKeyPressWithSearch(ui::VKEY_RIGHT);
    SendKeyPressWithSearch(ui::VKEY_RIGHT);
  });

  sm()->ExpectSpeech("Line 2");
  sm()->ExpectSpeech("Line 3");

  sm()->Call([this, &focus_bounds]() {
    WaitForMagnifierViewportOnBounds(focus_bounds);
  });

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackWithMagnifierTest,
                       FullscreenMagnifierTable) {
  gfx::Rect focus_bounds;

  sm()->Call([this, &focus_bounds]() {
    test_utils()->SetUpTestSupport();

    // Load a page with non-interactive table that would not get keyboard
    // focus.
    NavigateToUrl(GURL(R"(data:text/html;charset=utf-8,
        <table>
          <tr>
            <th>Heading 1</th>
            <th>Heading 2</th>
            <th>Heading 3</th>
            <th>Heading 4</th>
          </tr>
          <tr>
            <td>Cell 1</td>
            <td>Cell 2</td>
            <td>Cell 3</td>
            <td>Cell 4</td>
          </tr>
        </table>)"));

    // Set magnifier scale to something quite big so that the initial bounds
    // of the button are not within the magnifier bounds.
    AccessibilityManager::Get()->profile()->GetPrefs()->SetDouble(
        prefs::kAccessibilityScreenMagnifierScale, 8.0);

    focus_bounds = test_utils()->GetNodeBoundsInRoot("Heading 1", "staticText");
    EnsureMagnifierViewportNotOnBounds(focus_bounds);

    // Press T key to focus the table.
    SendKeyPressWithSearch(ui::VKEY_T);
  });

  sm()->ExpectSpeech("Heading 1");

  sm()->Call([this, &focus_bounds]() {
    WaitForMagnifierViewportOnBounds(focus_bounds);

    focus_bounds = test_utils()->GetNodeBoundsInRoot("Heading 4", "staticText");
    EnsureMagnifierViewportNotOnBounds(focus_bounds);

    // Press right key to focus the last heading which should not be currently
    // in the viewport.
    SendKeyPressWithSearch(ui::VKEY_RIGHT);
    SendKeyPressWithSearch(ui::VKEY_RIGHT);
    SendKeyPressWithSearch(ui::VKEY_RIGHT);
  });

  sm()->ExpectSpeech("Heading 2");
  sm()->ExpectSpeech("Heading 3");
  sm()->ExpectSpeech("Heading 4");

  sm()->Call([this, &focus_bounds]() {
    WaitForMagnifierViewportOnBounds(focus_bounds);
  });

  sm()->Replay();
}

}  // namespace ash
