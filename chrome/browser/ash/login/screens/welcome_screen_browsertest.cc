// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/welcome_screen.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_paths.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/screens/chromevox_hint/chromevox_hint_detector.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/display/test/display_manager_test_api.h"

namespace ash {

namespace {

const char kStartupManifestEnglish[] =
    R"({
      "version": "1.0",
      "initial_locale" : "en-US",
      "initial_timezone" : "US/Pacific",
      "keyboard_layout" : "xkb:us::eng",
    })";

const char kStartupManifestFrench[] =
    R"({
      "version": "1.0",
      "initial_locale" : "fr-FR",
      "initial_timezone" : "Europe/Paris",
      "keyboard_layout" : "xkb:fr::fra",
    })";

const char kCurrentLang[] =
    R"(document.getElementById('connect').$.welcomeScreen.currentLanguage)";
const char kCurrentKeyboard[] =
    R"(document.getElementById('connect').currentKeyboard)";

const test::UIPath kChromeVoxHintDialog = {"connect", "welcomeScreen",
                                           "chromeVoxHint"};
const test::UIPath kChromeVoxHintDialogCloseButton = {
    "connect", "welcomeScreen", "dismissChromeVoxButton"};
const test::UIPath kChromeVoxHintDialogContentClamshell = {
    "connect", "welcomeScreen", "chromeVoxHintContentClamshell"};
const test::UIPath kChromeVoxHintDialogContentTablet = {
    "connect", "welcomeScreen", "chromeVoxHintContentTablet"};
const test::UIPath kChromeVoxHintDialogTitle = {"connect", "welcomeScreen",
                                                "chromeVoxHintTitle"};
const test::UIPath kDismissChromeVoxButton = {"connect", "welcomeScreen",
                                              "dismissChromeVoxButton"};
const test::UIPath kActivateChromeVoxButton = {"connect", "welcomeScreen",
                                               "activateChromeVoxButton"};

const char kSetAvailableVoices[] = R"(
      chrome.tts.getVoices = function(callback) {
        callback([
          {'lang': 'en-US', 'voiceName': 'ChromeOS US English'},
          {'lang': 'fr-FR', 'voiceName': 'ChromeOS français'}
        ]);
      };)";

const char kChromeVoxHintLaptopSpokenStringImproved[] =
    "The screen reader on ChromeOS, ChromeVox, is primarily used by "
    "people with blindness or low vision to read text displayed on the screen "
    "with a speech synthesizer or braille display. Press the space bar to turn "
    "on "
    "ChromeVox. When ChromeVox is activated, you’ll go through a quick "
    "tour.";

const char kChromeVoxHintTabletSpokenStringImproved[] =
    "The screen reader on ChromeOS, ChromeVox, is primarily used by "
    "people with blindness or low vision to read text displayed on the screen "
    "with a speech synthesizer or braille display. Press and hold both volume "
    "keys "
    "for five seconds to turn on ChromeVox. When ChromeVox is activated, "
    "you’ll go through a quick tour.";

constexpr const char kWelcomeScreenLocaleChangeMetric[] =
    "OOBE.WelcomeScreen.UserChangedLocale";

void ToggleAccessibilityFeature(const std::string& feature_name,
                                bool new_value) {
  test::JSChecker js = test::OobeJS();
  std::string feature_toggle =
      test::GetOobeElementPath({"connect", feature_name, "button"}) +
      ".checked";

  if (!new_value)
    feature_toggle = "!" + feature_toggle;

  js.ExpectVisiblePath({"connect", feature_name, "button"});
  EXPECT_FALSE(js.GetBool(feature_toggle));
  js.TapOnPath({"connect", feature_name, "button"});
  js.CreateWaiter(feature_toggle)->Wait();
}

// Forces all css transitions to 0s duration.
void DisableCssTransitions() {
  test::OobeJS().Evaluate("document.body.style['transition']='all 0s ease 0s'");
}

}  // namespace

class WelcomeScreenBrowserTest : public OobeBaseTest {
 public:
  WelcomeScreenBrowserTest() = default;
  ~WelcomeScreenBrowserTest() override = default;

  // OobeBaseTest:
  bool SetUpUserDataDirectory() override {
    if (!OobeBaseTest::SetUpUserDataDirectory())
      return false;

    EXPECT_TRUE(data_dir_.CreateUniqueTempDir());
    const base::FilePath startup_manifest =
        data_dir_.GetPath().AppendASCII("startup_manifest.json");
    EXPECT_TRUE(base::WriteFile(startup_manifest, kStartupManifestEnglish));
    path_override_ = std::make_unique<base::ScopedPathOverride>(
        FILE_STARTUP_CUSTOMIZATION_MANIFEST, startup_manifest,
        /*is_absolute=*/false, /*create=*/false);

    // Make sure chrome paths are overridden before proceeding - this is usually
    // done in chrome main, which has not happened yet.
    base::FilePath user_data_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
    RegisterStubPathOverrides(user_data_dir);

    return true;
  }

  WelcomeScreen* welcome_screen() {
    EXPECT_NE(WizardController::default_controller(), nullptr);
    WelcomeScreen* welcome_screen =
        WizardController::default_controller()->GetScreen<WelcomeScreen>();
    EXPECT_NE(welcome_screen, nullptr);
    return welcome_screen;
  }

  void WaitForScreenExit() {
    OobeScreenExitWaiter(WelcomeView::kScreenId).Wait();
  }

  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<base::ScopedPathOverride> path_override_;
  base::ScopedTempDir data_dir_;
};

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, WelcomeScreenElements) {
  test::WaitForWelcomeScreen();

  test::OobeJS().ExpectVisiblePath({"connect", "welcomeScreen"});
  test::OobeJS().ExpectHiddenPath({"connect", "accessibilityScreen"});
  test::OobeJS().ExpectHiddenPath({"connect", "languageScreen"});
  test::OobeJS().ExpectHiddenPath({"connect", "timezoneScreen"});
  test::OobeJS().ExpectVisiblePath({"connect", "welcomeScreen", "getStarted"});
  test::OobeJS().ExpectVisiblePath(
      {"connect", "welcomeScreen", "languageSelectionButton"});
  test::OobeJS().ExpectVisiblePath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});
  test::OobeJS().ExpectHiddenPath(
      {"connect", "welcomeScreen", "timezoneSettingsButton"});
  test::OobeJS().ExpectHiddenPath(
      {"connect", "welcomeScreen", "enableDebuggingButton"});
}

// This is a minimal possible test for OOBE. It is used as reference test
// for measurements during OOBE speedup work. Also verifies OOBE.WebUI.LoadTime
// metric.
IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, OobeStartupTime) {
  test::WaitForWelcomeScreen();
  histogram_tester_.ExpectTotalCount("OOBE.WebUI.LoadTime.FirstRun", 1);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, WelcomeScreenNext) {
  test::WaitForWelcomeScreen();
  histogram_tester_.ExpectTotalCount("OOBE.StepShownStatus2.Connect", 1);
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "getStarted"});
  WaitForScreenExit();
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime2.Connect", 1);
}

// Set of browser tests for Welcome Screen Language options.
IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, WelcomeScreenLanguageFlow) {
  test::WaitForWelcomeScreen();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "languageSelectionButton"});

  test::OobeJS().TapOnPath({"connect", "ok-button-language"});
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "getStarted"});
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenLanguageElements) {
  test::WaitForWelcomeScreen();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "languageSelectionButton"});

  test::OobeJS().ExpectVisiblePath({"connect", "languageDropdownContainer"});
  test::OobeJS().ExpectVisiblePath({"connect", "keyboardDropdownContainer"});
  test::OobeJS().ExpectVisiblePath({"connect", "languageSelect"});
  test::OobeJS().ExpectVisiblePath({"connect", "keyboardSelect"});
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenLanguageSelection) {
  test::WaitForWelcomeScreen();

  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "languageSelectionButton"});
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");

  test::OobeJS().ExpectEQ(kCurrentLang, std::string("English (United States)"));

  {
    test::LanguageReloadObserver observer(welcome_screen());
    test::OobeJS().SelectElementInPath("fr",
                                       {"connect", "languageSelect", "select"});
    observer.Wait();
    test::OobeJS().ExpectEQ(kCurrentLang, std::string("français"));
    EXPECT_EQ(g_browser_process->GetApplicationLocale(), "fr");
  }

  {
    test::LanguageReloadObserver observer(welcome_screen());
    test::OobeJS().SelectElementInPath("en-US",
                                       {"connect", "languageSelect", "select"});
    observer.Wait();
    test::OobeJS().ExpectEQ(kCurrentLang,
                            std::string("English (United States)"));
    EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");
  }
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenKeyboardSelection) {
  test::WaitForWelcomeScreen();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "languageSelectionButton"});

  extensions::ExtensionId extension_id_prefix =
      std::string("_comp_ime_") + extension_ime_util::kXkbExtensionId;

  test::OobeJS().SelectElementInPath(extension_id_prefix + "xkb:us:intl:eng",
                                     {"connect", "keyboardSelect", "select"});
  test::OobeJS().ExpectEQ(kCurrentKeyboard, std::string("US international"));
  ASSERT_EQ(welcome_screen()->GetInputMethod(),
            extension_id_prefix + "xkb:us:intl:eng");

  test::OobeJS().SelectElementInPath(extension_id_prefix + "xkb:us:workman:eng",
                                     {"connect", "keyboardSelect", "select"});
  test::OobeJS().ExpectEQ(kCurrentKeyboard, std::string("US Workman"));
  ASSERT_EQ(welcome_screen()->GetInputMethod(),
            extension_id_prefix + "xkb:us:workman:eng");
}

// Set of browser tests for Welcome Screen Accessibility options.
IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilityFlow) {
  test::WaitForWelcomeScreen();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  test::OobeJS().TapOnPath({"connect", "ok-button-accessibility"});
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "getStarted"});
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilitySpokenFeedback) {
  test::WaitForWelcomeScreen();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  ToggleAccessibilityFeature("accessibility-spoken-feedback", true);
  ASSERT_TRUE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kEnableSpokenFeedback, 1);

  ToggleAccessibilityFeature("accessibility-spoken-feedback", false);
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kDisableSpokenFeedback, 1);

  histogram_tester_.ExpectTotalCount("OOBE.WelcomeScreen.A11yUserActions", 2);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilityLargeCursor) {
  test::WaitForWelcomeScreen();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(AccessibilityManager::Get()->IsLargeCursorEnabled());
  ToggleAccessibilityFeature("accessibility-large-cursor", true);
  ASSERT_TRUE(AccessibilityManager::Get()->IsLargeCursorEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kEnableLargeCursor, 1);

  ToggleAccessibilityFeature("accessibility-large-cursor", false);
  ASSERT_FALSE(AccessibilityManager::Get()->IsLargeCursorEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kDisableLargeCursor, 1);

  histogram_tester_.ExpectTotalCount("OOBE.WelcomeScreen.A11yUserActions", 2);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilityHighContrast) {
  test::WaitForWelcomeScreen();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(AccessibilityManager::Get()->IsHighContrastEnabled());
  ToggleAccessibilityFeature("accessibility-high-contrast", true);
  ASSERT_TRUE(AccessibilityManager::Get()->IsHighContrastEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kEnableHighContrast, 1);

  ToggleAccessibilityFeature("accessibility-high-contrast", false);
  ASSERT_FALSE(AccessibilityManager::Get()->IsHighContrastEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kDisableHighContrast, 1);

  histogram_tester_.ExpectTotalCount("OOBE.WelcomeScreen.A11yUserActions", 2);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilitySelectToSpeak) {
  test::WaitForWelcomeScreen();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());
  ToggleAccessibilityFeature("accessibility-select-to-speak", true);
  ASSERT_TRUE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kEnableSelectToSpeak, 1);

  ToggleAccessibilityFeature("accessibility-select-to-speak", false);
  ASSERT_FALSE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kDisableSelectToSpeak, 1);

  histogram_tester_.ExpectTotalCount("OOBE.WelcomeScreen.A11yUserActions", 2);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilityScreenMagnifier) {
  test::WaitForWelcomeScreen();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(MagnificationManager::Get()->IsMagnifierEnabled());
  ToggleAccessibilityFeature("accessibility-screen-magnifier", true);
  ASSERT_TRUE(MagnificationManager::Get()->IsMagnifierEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kEnableScreenMagnifier, 1);

  ToggleAccessibilityFeature("accessibility-screen-magnifier", false);
  ASSERT_FALSE(MagnificationManager::Get()->IsMagnifierEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kDisableScreenMagnifier, 1);

  histogram_tester_.ExpectTotalCount("OOBE.WelcomeScreen.A11yUserActions", 2);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilityDockedMagnifier) {
  test::WaitForWelcomeScreen();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(MagnificationManager::Get()->IsDockedMagnifierEnabled());
  ToggleAccessibilityFeature("accessibility-docked-magnifier", true);
  ASSERT_TRUE(MagnificationManager::Get()->IsDockedMagnifierEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kEnableDockedMagnifier, 1);

  ToggleAccessibilityFeature("accessibility-docked-magnifier", false);
  ASSERT_FALSE(MagnificationManager::Get()->IsDockedMagnifierEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kDisableDockedMagnifier, 1);

  histogram_tester_.ExpectTotalCount("OOBE.WelcomeScreen.A11yUserActions", 2);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, PRE_SelectedLanguage) {
  EXPECT_EQ(
      StartupCustomizationDocument::GetInstance()->initial_locale_default(),
      "en-US");
  test::WaitForWelcomeScreen();
  const std::string locale = "ru";
  test::LanguageReloadObserver observer(welcome_screen());
  welcome_screen()->SetApplicationLocale(locale, /*is_from_ui*/ true);
  observer.Wait();

  EXPECT_EQ(g_browser_process->local_state()->GetString(
                language::prefs::kApplicationLocale),
            locale);
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), locale);

  // We need to proceed otherwise welcome screen would reset language on the
  // next show.
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "getStarted"});
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, SelectedLanguage) {
  const std::string locale = "ru";
  EXPECT_EQ(g_browser_process->local_state()->GetString(
                language::prefs::kApplicationLocale),
            locale);
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), locale);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, A11yVirtualKeyboard) {
  test::WaitForWelcomeScreen();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(AccessibilityManager::Get()->IsVirtualKeyboardEnabled());
  ToggleAccessibilityFeature("accessibility-virtual-keyboard", true);
  ASSERT_TRUE(AccessibilityManager::Get()->IsVirtualKeyboardEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kEnableVirtualKeyboard, 1);

  ToggleAccessibilityFeature("accessibility-virtual-keyboard", false);
  ASSERT_FALSE(AccessibilityManager::Get()->IsVirtualKeyboardEnabled());
  histogram_tester_.ExpectBucketCount(
      "OOBE.WelcomeScreen.A11yUserActions",
      WelcomeScreen::A11yUserAction::kDisableVirtualKeyboard, 1);

  histogram_tester_.ExpectTotalCount("OOBE.WelcomeScreen.A11yUserActions", 2);
}

// Set of tests for the OOBE.WelcomeScreen.UserChangedLocale
IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, UserChangedLocaleMetric) {
  test::WaitForWelcomeScreen();
  // We need to proceed to the next screen because metric is written right
  // before we call exit_callback_().
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "getStarted"});
  WaitForScreenExit();
  histogram_tester_.ExpectTotalCount(kWelcomeScreenLocaleChangeMetric, 1);
  histogram_tester_.ExpectUniqueSample(kWelcomeScreenLocaleChangeMetric, false,
                                       1);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       UserChangedLocaleMetricAfterUILocaleChange) {
  test::WaitForWelcomeScreen();

  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "languageSelectionButton"});
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");

  test::OobeJS().ExpectEQ(kCurrentLang, std::string("English (United States)"));

  test::LanguageReloadObserver observer(welcome_screen());
  test::OobeJS().SelectElementInPath("fr",
                                     {"connect", "languageSelect", "select"});
  observer.Wait();
  test::OobeJS().ExpectEQ(kCurrentLang, std::string("français"));
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "fr");

  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "getStarted"});
  WaitForScreenExit();

  histogram_tester_.ExpectTotalCount(kWelcomeScreenLocaleChangeMetric, 1);
  histogram_tester_.ExpectUniqueSample(kWelcomeScreenLocaleChangeMetric, true,
                                       1);
}

class WelcomeScreenInsetModeBrowserTest
    : public WelcomeScreenBrowserTest,
      public testing::WithParamInterface<std::tuple</*BootAnimation*/ bool,
                                                    /*OobeJelly*/ bool,
                                                    /*OobeJellyModal*/ bool>> {
 public:
  WelcomeScreenInsetModeBrowserTest() {
    const bool boot_animation = std::get<0>(GetParam());
    const bool oobe_jelly = std::get<1>(GetParam());
    const bool oobe_jelly_modal = std::get<2>(GetParam());

    scoped_feature_list_.InitWithFeatureStates(
        {{features::kFeatureManagementOobeSimon, boot_animation},
         {chromeos::features::kJelly, oobe_jelly},
         {features::kOobeJelly, oobe_jelly},
         {features::kOobeJellyModal, oobe_jelly_modal}});
  }
  ~WelcomeScreenInsetModeBrowserTest() override = default;

  // Populate meaningful test suffixes instead of /0, /1, etc.
  struct PrintToStringParamName {
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      std::stringstream ss;
      ss << std::get<0>(info.param) << "_AND_" << std::get<1>(info.param)
         << "_AND_" << std::get<2>(info.param);
      return ss.str();
    }
  };

  const std::string kGetCalculatedBackgroundColor =
      "window.getComputedStyle(document.body)"
      ".getPropertyValue('background-color')";
  const std::string kRgbaTransparent = "rgba(0, 0, 0, 0)";

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    WelcomeScreenInsetModeBrowserTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool(), ::testing::Bool()),
    WelcomeScreenInsetModeBrowserTest::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(WelcomeScreenInsetModeBrowserTest,
                       EnsureBackgroundOpacityForDifferentResolutions) {
  display::test::DisplayManagerTestApi display_manager(
      ash::ShellTestApi().display_manager());
  test::WaitForWelcomeScreen();
  DisableCssTransitions();

  // Closest value to threshold of 1040 px.
  // Check in UpdateDisplay fails if height==width
  display_manager.UpdateDisplay(std::string("1039x1038"));
  test::OobeJS().ExpectNE(kGetCalculatedBackgroundColor, kRgbaTransparent);

  // Use inset mode if one screen dimension is >=1040px (and tablet mode is off)
  display_manager.UpdateDisplay(std::string("600x1040"));
  if (ash::features::IsBootAnimationEnabled() ||
      ash::features::IsOobeJellyModalEnabled()) {
    test::OobeJS().ExpectEQ(kGetCalculatedBackgroundColor, kRgbaTransparent);
  } else {
    test::OobeJS().ExpectNE(kGetCalculatedBackgroundColor, kRgbaTransparent);
  }

  display_manager.UpdateDisplay(std::string("1040x600"));
  if (ash::features::IsBootAnimationEnabled() ||
      ash::features::IsOobeJellyModalEnabled()) {
    test::OobeJS().ExpectEQ(kGetCalculatedBackgroundColor, kRgbaTransparent);
  } else {
    test::OobeJS().ExpectNE(kGetCalculatedBackgroundColor, kRgbaTransparent);
  }
}

IN_PROC_BROWSER_TEST_P(WelcomeScreenInsetModeBrowserTest,
                       EnsureBackgroundOpacityForTabletMode) {
  display::test::DisplayManagerTestApi display_manager(
      ash::ShellTestApi().display_manager());
  test::WaitForWelcomeScreen();
  DisableCssTransitions();
  display_manager.UpdateDisplay(std::string("1040x600"));

  // Never use inset mode for tablets
  ShellTestApi().SetTabletModeEnabledForTest(true);
  test::OobeJS().ExpectNE(kGetCalculatedBackgroundColor, kRgbaTransparent);

  ShellTestApi().SetTabletModeEnabledForTest(false);
  if (ash::features::IsBootAnimationEnabled() ||
      ash::features::IsOobeJellyModalEnabled()) {
    test::OobeJS().ExpectEQ(kGetCalculatedBackgroundColor, kRgbaTransparent);
  } else {
    test::OobeJS().ExpectNE(kGetCalculatedBackgroundColor, kRgbaTransparent);
  }
}

class WelcomeScreenBootAnimationBrowserTest
    : public WelcomeScreenBrowserTest,
      public testing::WithParamInterface</*BootAnimation*/ bool> {
 public:
  WelcomeScreenBootAnimationBrowserTest() {
    const bool boot_animation = GetParam();

    scoped_feature_list_.InitWithFeatureStates(
        {{features::kFeatureManagementOobeSimon, boot_animation}});
  }
  ~WelcomeScreenBootAnimationBrowserTest() override = default;

  const std::string kGetBackdropDisplayValue =
      "window.getComputedStyle(document.querySelector('#welcome-backdrop'))"
      ".getPropertyValue('display')";
  const std::string kGetCalculatedBackgroundColorInnerContainer =
      "window.getComputedStyle(document.querySelector('#inner-container'))"
      ".getPropertyValue('background-color')";
  const std::string kRgbaTransparent = "rgba(0, 0, 0, 0)";

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WelcomeScreenBootAnimationBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(WelcomeScreenBootAnimationBrowserTest,
                       CheckBackdropVisibility) {
  test::WaitForWelcomeScreen();
  DisableCssTransitions();

  if (ash::features::IsBootAnimationEnabled()) {
    test::OobeJS().ExpectVisible("welcome-backdrop");
    test::OobeJS().ExpectEQ(kGetBackdropDisplayValue, std::string("block"));
    test::OobeJS().ExpectEQ(kGetCalculatedBackgroundColorInnerContainer,
                            kRgbaTransparent);
  } else {
    test::OobeJS().ExpectEQ(kGetBackdropDisplayValue, std::string("none"));
    test::OobeJS().ExpectNE(kGetCalculatedBackgroundColorInnerContainer,
                            kRgbaTransparent);
  }

  test::OobeJS().ClickOnPath(
      {"connect", "welcomeScreen", "languageSelectionButton"});
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"connect", "languageScreen"})
      ->Wait();
  test::OobeJS().ExpectEQ(kGetBackdropDisplayValue, std::string("none"));
  test::OobeJS().ExpectNE(kGetCalculatedBackgroundColorInnerContainer,
                          kRgbaTransparent);
  test::OobeJS().ClickOnPath({"connect", "ok-button-language"});
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"connect", "welcomeScreen"})
      ->Wait();

  test::OobeJS().ClickOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"connect", "accessibilityScreen"})
      ->Wait();
  test::OobeJS().ExpectEQ(kGetBackdropDisplayValue, std::string("none"));
  test::OobeJS().ExpectNE(kGetCalculatedBackgroundColorInnerContainer,
                          kRgbaTransparent);
}

class WelcomeScreenSystemDevModeBrowserTest : public WelcomeScreenBrowserTest {
 public:
  WelcomeScreenSystemDevModeBrowserTest() = default;
  ~WelcomeScreenSystemDevModeBrowserTest() override = default;

  // WelcomeScreenBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WelcomeScreenBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kSystemDevMode);
  }
};

IN_PROC_BROWSER_TEST_F(WelcomeScreenSystemDevModeBrowserTest,
                       DebuggerModeTest) {
  test::WaitForWelcomeScreen();
  test::OobeJS().ClickOnPath(
      {"connect", "welcomeScreen", "enableDebuggingButton"});

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"debugging", "removeProtectionDialog"})
      ->Wait();
  test::OobeJS().ExpectVisiblePath(
      {"debugging", "removeProtectionProceedButton"});
  test::OobeJS().ExpectVisiblePath(
      {"debugging", "removeProtectionCancelButton"});
  test::OobeJS().ExpectVisiblePath({"debugging", "help-link"});
  test::OobeJS().ClickOnPath({"debugging", "removeProtectionCancelButton"});
}

class WelcomeScreenTimezone : public WelcomeScreenBrowserTest {
 public:
  WelcomeScreenTimezone() {
    fake_statistics_provider_.SetMachineFlag(system::kOemKeyboardDrivenOobeKey,
                                             true);
  }

  WelcomeScreenTimezone(const WelcomeScreenTimezone&) = delete;
  WelcomeScreenTimezone& operator=(const WelcomeScreenTimezone&) = delete;

 protected:
  void CheckTimezone(const std::string& timezone) {
    std::string system_timezone;
    CrosSettings::Get()->GetString(kSystemTimezone, &system_timezone);
    EXPECT_EQ(timezone, system_timezone);

    const std::string signin_screen_timezone =
        g_browser_process->local_state()->GetString(
            prefs::kSigninScreenTimezone);
    EXPECT_EQ(timezone, signin_screen_timezone);
  }

 private:
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

IN_PROC_BROWSER_TEST_F(WelcomeScreenTimezone, ChangeTimezoneFlow) {
  test::WaitForWelcomeScreen();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "timezoneSettingsButton"});

  std::string system_timezone;
  CrosSettings::Get()->GetString(kSystemTimezone, &system_timezone);
  const char kTestTimezone[] = "Asia/Novosibirsk";
  ASSERT_NE(kTestTimezone, system_timezone);

  test::OobeJS().SelectElementInPath(kTestTimezone,
                                     {"connect", "timezoneSelect", "select"});
  CheckTimezone(kTestTimezone);
  test::OobeJS().TapOnPath({"connect", "ok-button-timezone"});
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "getStarted"});
  WaitForScreenExit();

  // Must not change.
  CheckTimezone(kTestTimezone);
}

class WelcomeScreenChromeVoxHintTest : public WelcomeScreenBrowserTest {
 public:
  WelcomeScreenChromeVoxHintTest() = default;
  ~WelcomeScreenChromeVoxHintTest() override = default;

  void WaitForChromeVoxHintDialogToOpen() {
    test::OobeJS()
        .CreateWaiter(test::GetOobeElementPath(kChromeVoxHintDialog) + ".open")
        ->Wait();
  }

  void WaitForChromeVoxHintDialogToClose() {
    test::OobeJS()
        .CreateWaiter(test::GetOobeElementPath(kChromeVoxHintDialog) +
                      ".open === false")
        ->Wait();
  }

  void WaitForSpokenSuccessMetric() {
    test::TestPredicateWaiter(
        base::BindRepeating(
            [](base::HistogramTester* tester) {
              return tester->GetBucketCount(
                         "OOBE.WelcomeScreen.ChromeVoxHintSpokenSuccess",
                         true) == 1;
            },
            &histogram_tester_))
        .Wait();
    histogram_tester_.ExpectUniqueSample(
        "OOBE.WelcomeScreen.ChromeVoxHintSpokenSuccess", true, 1);
  }

  void AssertChromeVoxHintDetector() {
    ASSERT_TRUE(welcome_screen()->GetChromeVoxHintDetectorForTesting());
  }

  void GiveChromeVoxHintForTesting() {
    AssertChromeVoxHintDetector();
    welcome_screen()->GetChromeVoxHintDetectorForTesting()->OnIdle();
  }

  bool IdleDetectionActivatedForTesting() {
    AssertChromeVoxHintDetector();
    return !!welcome_screen()
                 ->GetChromeVoxHintDetectorForTesting()
                 ->idle_detector_;
  }

  bool IdleDetectionCancelledForTesting() {
    return !(welcome_screen()->GetChromeVoxHintDetectorForTesting());
  }
};

// Assert that the ChromeVox hint gives speech output and shows a dialog.
// Clicking the 'activate' button in the dialog should activate ChromeVox.
// crbug.com/1341515 Disabled due to flakiness.
IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest, DISABLED_LaptopClick) {
  test::WaitForWelcomeScreen();
  // A consistency check to ensure the ChromeVox hint idle detector is disabled
  // for this and similar tests.
  ASSERT_FALSE(IdleDetectionActivatedForTesting());
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  test::ExecuteOobeJS(kSetAvailableVoices);
  test::SpeechMonitor monitor;
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
  GiveChromeVoxHintForTesting();
  // A consistency check to ensure we stop idle detection after the hint is
  // given.
  ASSERT_TRUE(IdleDetectionCancelledForTesting());
  monitor.ExpectSpeech(kChromeVoxHintLaptopSpokenStringImproved);
  monitor.Call([this]() {
    ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
    WaitForChromeVoxHintDialogToOpen();
    test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, true);
    test::OobeJS().ClickOnPath(kActivateChromeVoxButton);
  });
  monitor.ExpectSpeechPattern("*");
  monitor.Call([this]() {
    ASSERT_TRUE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
    WaitForChromeVoxHintDialogToClose();
    test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
    histogram_tester_.ExpectUniqueSample(
        "OOBE.WelcomeScreen.AcceptChromeVoxHint", true, 1);
  });
  monitor.Replay();
  WaitForSpokenSuccessMetric();
}

// Assert that the ChromeVox hint gives speech output and shows a dialog.
// Pressing the space bar while the dialog is open should activate ChromeVox.
IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest, LaptopSpaceBar) {
  test::WaitForWelcomeScreen();
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  test::ExecuteOobeJS(kSetAvailableVoices);
  test::SpeechMonitor monitor;
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
  GiveChromeVoxHintForTesting();
  monitor.ExpectSpeech(kChromeVoxHintLaptopSpokenStringImproved);
  monitor.Call([this]() {
    ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
    WaitForChromeVoxHintDialogToOpen();
    test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, true);
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, ui::VKEY_SPACE, false /* control */, false /* shift */,
        false /* alt */, false /* command */));
  });
  monitor.ExpectSpeechPattern("*");
  monitor.Call([this]() {
    ASSERT_TRUE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
    WaitForChromeVoxHintDialogToClose();
    test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
    histogram_tester_.ExpectUniqueSample(
        "OOBE.WelcomeScreen.AcceptChromeVoxHint", true, 1);
  });
  monitor.Replay();
  WaitForSpokenSuccessMetric();
}

// Tests the ChromeVox hint speech given in tablet mode.
IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest, Tablet) {
  test::WaitForWelcomeScreen();
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  test::ExecuteOobeJS(kSetAvailableVoices);
  ShellTestApi().SetTabletModeEnabledForTest(true);
  test::SpeechMonitor monitor;
  GiveChromeVoxHintForTesting();
  monitor.ExpectSpeech(kChromeVoxHintTabletSpokenStringImproved);
  monitor.Replay();
  WaitForSpokenSuccessMetric();
}

// Tests that the ChromeVox hint can be spoken, even if the necessary voice
// hasn't loaded when the idle detector has fired.
// TODO(b/283990894) - Re-enable test.
IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest, DISABLED_VoicesChanged) {
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  const std::string set_no_english_voice = R"(
  chrome.tts.getVoices = function(callback) {
    callback([{'lang': 'fr-FR', 'voiceName': 'ChromeOS français'}]);
  };)";
  test::ExecuteOobeJS(set_no_english_voice);

  test::WaitForWelcomeScreen();

  test::SpeechMonitor monitor;
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
  GiveChromeVoxHintForTesting();
  // Wait for voiceschanged listener to register.
  test::OobeJS()
      .CreateWaiter(
          "document.getElementById('connect')."
          "voicesChangedListenerMaybeGiveChromeVoxHint !== undefined")
      ->Wait();
  const std::string load_english_voice = R"(
    chrome.tts.getVoices = function(callback) {
      callback([
        {'lang': 'fr-FR', 'voiceName': 'ChromeOS français'},
        {'lang': 'en-US', 'voiceName': 'ChromeOS US English'},
      ]);
    };
    window.speechSynthesis.dispatchEvent(new Event('voiceschanged'));
    )";
  test::ExecuteOobeJS(load_english_voice);
  monitor.ExpectSpeech(kChromeVoxHintLaptopSpokenStringImproved);
  monitor.Replay();
  WaitForSpokenSuccessMetric();
}

// Assert that clicking on one of the three buttons on the welcome screen
// cancels the ChromeVox hint.
IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest, CancelHint) {
  test::WaitForWelcomeScreen();
  ASSERT_FALSE(IdleDetectionCancelledForTesting());
  test::OobeJS().ClickOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});
  ASSERT_TRUE(IdleDetectionCancelledForTesting());
}

#if !defined(NDEBUG)
#define MAYBE_ActivateChromeVoxBeforeHint DISABLED_ActivateChromeVoxBeforeHint
#else
#define MAYBE_ActivateChromeVoxBeforeHint ActivateChromeVoxBeforeHint
#endif
// Assert that activating ChromeVox before the hint cancels the hint's idle
// timeout.
IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest,
                       MAYBE_ActivateChromeVoxBeforeHint) {
  test::WaitForWelcomeScreen();
  ASSERT_FALSE(IdleDetectionCancelledForTesting());
  ToggleAccessibilityFeature("accessibility-spoken-feedback", true);
  ASSERT_TRUE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  ASSERT_TRUE(IdleDetectionCancelledForTesting());
}

// Assert that activating ChromeVox (after the hint is given) closes the hint
// dialog.
IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest,
                       DISABLED_ActivateChromeVoxAfterHint) {
  test::WaitForWelcomeScreen();
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  test::ExecuteOobeJS(kSetAvailableVoices);
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
  GiveChromeVoxHintForTesting();
  WaitForChromeVoxHintDialogToOpen();
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, true);
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  ASSERT_TRUE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  WaitForChromeVoxHintDialogToClose();
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
}

// Assert that we can dismiss the ChromeVox hint dialog and that the appropriate
// metrics get recorded.
IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest, DismissAfterHint) {
  test::WaitForWelcomeScreen();
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  test::ExecuteOobeJS(kSetAvailableVoices);
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
  GiveChromeVoxHintForTesting();
  WaitForChromeVoxHintDialogToOpen();
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, true);
  test::OobeJS().ClickOnPath(kDismissChromeVoxButton);
  WaitForChromeVoxHintDialogToClose();
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
  histogram_tester_.ExpectUniqueSample("OOBE.WelcomeScreen.AcceptChromeVoxHint",
                                       false, 1);
}

// Assert that the ChromeVox hint dialog behaves as a modal dialog and traps
// focus when using tab.
// TODO(crbug/1161398): The test is flaky.
IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest, DISABLED_TrapFocus) {
  test::WaitForWelcomeScreen();
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  test::ExecuteOobeJS(kSetAvailableVoices);
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
  GiveChromeVoxHintForTesting();
  WaitForChromeVoxHintDialogToOpen();
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, true);

  // Ensure that focus stays inside the dialog's context.
  // Move forward through the tab order by pressing tab.
  test::OobeJS().CreateFocusWaiter(kChromeVoxHintDialog)->Wait();
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::VKEY_TAB, false /* control */, false /* shift */,
      false /* alt */, false /* command */));
  test::OobeJS().CreateFocusWaiter(kDismissChromeVoxButton)->Wait();
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::VKEY_TAB, false /* control */, false /* shift */,
      false /* alt */, false /* command */));
  test::OobeJS().CreateFocusWaiter(kActivateChromeVoxButton)->Wait();
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::VKEY_TAB, false /* control */, false /* shift */,
      false /* alt */, false /* command */));
  test::OobeJS().CreateFocusWaiter(kDismissChromeVoxButton)->Wait();

  // Move backward by pressing shift + tab.
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::VKEY_TAB, false /* control */, true /* shift */,
      false /* alt */, false /* command */));
  test::OobeJS().CreateFocusWaiter(kActivateChromeVoxButton)->Wait();
}

// Verifies that the ChromeVox idle detector is cancelled when
// skipToLoginForTesting is called.
IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest, SkipToLoginForTesting) {
  test::WaitForWelcomeScreen();
  WizardController::default_controller()->SkipToLoginForTesting();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();

  EXPECT_TRUE(IdleDetectionCancelledForTesting());
}

// Verifies that the ChromeVox idle detector is cancelled when the status tray
// is shown.
IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest, StatusTray) {
  test::WaitForWelcomeScreen();
  ASSERT_FALSE(IdleDetectionCancelledForTesting());
  SystemTrayTestApi::Create()->ShowBubble();
  ASSERT_TRUE(IdleDetectionCancelledForTesting());
}

// Verifies that TTS output stops after the user has closed the dialog.
IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest, StopSpeechAfterClose) {
  test::WaitForWelcomeScreen();
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  test::ExecuteOobeJS(kSetAvailableVoices);
  const std::string set_is_speaking = R"(
    chrome.tts.isSpeaking = function(callback) {
      callback(true);
    };)";
  test::ExecuteOobeJS(set_is_speaking);
  test::SpeechMonitor monitor;
  GiveChromeVoxHintForTesting();
  WaitForChromeVoxHintDialogToOpen();
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, true);
  int expected_stop_count = monitor.stop_count() + 1;
  test::OobeJS().ClickOnPath(kDismissChromeVoxButton);
  ASSERT_EQ(expected_stop_count, monitor.stop_count());
}

class WelcomeScreenInternationalChromeVoxHintTest
    : public WelcomeScreenChromeVoxHintTest {
 public:
  bool SetUpUserDataDirectory() override {
    if (!OobeBaseTest::SetUpUserDataDirectory())
      return false;
    EXPECT_TRUE(data_dir_.CreateUniqueTempDir());
    const base::FilePath startup_manifest =
        data_dir_.GetPath().AppendASCII("startup_manifest.json");
    EXPECT_TRUE(base::WriteFile(startup_manifest, kStartupManifestFrench));
    path_override_ = std::make_unique<base::ScopedPathOverride>(
        FILE_STARTUP_CUSTOMIZATION_MANIFEST, startup_manifest,
        /*is_absolute=*/false, /*create=*/false);
    return true;
  }

 private:
  std::unique_ptr<base::ScopedPathOverride> path_override_;
  base::ScopedTempDir data_dir_;
};

// Tests the ChromeVox hint speech can be given in a language other than
// English.
IN_PROC_BROWSER_TEST_F(WelcomeScreenInternationalChromeVoxHintTest, SpeakHint) {
  test::WaitForWelcomeScreen();
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  test::ExecuteOobeJS(kSetAvailableVoices);
  test::SpeechMonitor monitor;
  GiveChromeVoxHintForTesting();
  monitor.ExpectSpeech(
      test::SpeechMonitor::Expectation("*").AsPattern().WithLocale("fr"));
  monitor.Replay();
  WaitForSpokenSuccessMetric();
}

// Tests that the ChromeVox hint is spoken in English (after a timeout) if no
// available voice can be loaded.
IN_PROC_BROWSER_TEST_F(WelcomeScreenInternationalChromeVoxHintTest,
                       DefaultAnnouncement) {
  test::WaitForWelcomeScreen();
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  // Load an English voice, but do not load a French voice.
  // Also set the timeout for the fallback hint to 0 MS.
  const std::string set_no_french_voice = R"(
    chrome.tts.getVoices = function(callback) {
      callback([{'lang': 'en-US', 'voiceName': 'ChromeOS US English'}]);
    };)";
  const std::string set_default_hint_timeout_ms = R"(
    document.getElementById('connect').DEFAULT_CHROMEVOX_HINT_TIMEOUT_MS = 0;
    )";
  test::ExecuteOobeJS(set_default_hint_timeout_ms);
  test::ExecuteOobeJS(set_no_french_voice);
  test::SpeechMonitor monitor;
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
  GiveChromeVoxHintForTesting();
  // Expect speech in English, even though the system locale is French.
  monitor.ExpectSpeech(
      test::SpeechMonitor::Expectation("*").AsPattern().WithLocale("en-US"));
  monitor.Replay();
  WaitForSpokenSuccessMetric();
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest,
                       DialogStructureClamshell) {
  test::WaitForWelcomeScreen();
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  test::ExecuteOobeJS(kSetAvailableVoices);
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
  GiveChromeVoxHintForTesting();
  WaitForChromeVoxHintDialogToOpen();
  test::OobeJS().ExpectAttributeEQ("textContent", kChromeVoxHintDialogTitle,
                                   std::string("Turn on screen reader"));
  test::OobeJS().ExpectAttributeEQ(
      "textContent", kChromeVoxHintDialogContentClamshell,
      std::string(kChromeVoxHintLaptopSpokenStringImproved));
  // Tablet content should not be displayed.
  test::OobeJS().ExpectPathDisplayed(false, kChromeVoxHintDialogContentTablet);
  test::OobeJS().ExpectAttributeEQ(
      "labelForAria_", kChromeVoxHintDialogCloseButton, std::string("Close"));
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest, DialogStructureTablet) {
  test::WaitForWelcomeScreen();
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  test::ExecuteOobeJS(kSetAvailableVoices);
  ShellTestApi().SetTabletModeEnabledForTest(true);
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
  GiveChromeVoxHintForTesting();
  WaitForChromeVoxHintDialogToOpen();
  test::OobeJS().ExpectAttributeEQ("textContent", kChromeVoxHintDialogTitle,
                                   std::string("Turn on screen reader"));
  test::OobeJS().ExpectAttributeEQ(
      "textContent", kChromeVoxHintDialogContentTablet,
      std::string(kChromeVoxHintTabletSpokenStringImproved));
  // Clamshell content should not be displayed.
  test::OobeJS().ExpectPathDisplayed(false,
                                     kChromeVoxHintDialogContentClamshell);
  test::OobeJS().ExpectAttributeEQ(
      "labelForAria_", kChromeVoxHintDialogCloseButton, std::string("Close"));
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest, ClamshellAnnouncement) {
  test::WaitForWelcomeScreen();
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  test::ExecuteOobeJS(kSetAvailableVoices);
  test::SpeechMonitor monitor;
  GiveChromeVoxHintForTesting();
  monitor.ExpectSpeech(kChromeVoxHintLaptopSpokenStringImproved);
  monitor.Replay();
  WaitForSpokenSuccessMetric();
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenChromeVoxHintTest, TabletAnnouncement) {
  test::WaitForWelcomeScreen();
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
  test::ExecuteOobeJS(kSetAvailableVoices);
  ShellTestApi().SetTabletModeEnabledForTest(true);
  test::SpeechMonitor monitor;
  GiveChromeVoxHintForTesting();
  monitor.ExpectSpeech(kChromeVoxHintTabletSpokenStringImproved);
  monitor.Replay();
  WaitForSpokenSuccessMetric();
}

}  // namespace ash
