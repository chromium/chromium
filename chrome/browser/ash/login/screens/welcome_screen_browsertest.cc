// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_paths.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/screens/chromevox_hint/chromevox_hint_detector.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/extension_ime_util.h"

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

const char kChromeVoxHintLaptopSpokenString[] =
    "Do you want to activate ChromeVox, the built-in screen reader for "
    "ChromeOS? If so, press the space bar.";

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
        FILE_STARTUP_CUSTOMIZATION_MANIFEST, startup_manifest);

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
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "getStarted"});
  WaitForScreenExit();
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

  std::string extension_id_prefix =
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

class WelcomeScreenHandsOffBrowserTest : public WelcomeScreenBrowserTest {
 public:
  WelcomeScreenHandsOffBrowserTest() = default;
  ~WelcomeScreenHandsOffBrowserTest() override = default;

  // WelcomeScreenBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WelcomeScreenBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");
  }
};

IN_PROC_BROWSER_TEST_F(WelcomeScreenHandsOffBrowserTest, SkipScreen) {
  WaitForScreenExit();
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
        .CreateWaiter(test::GetOobeElementPath({kChromeVoxHintDialog}) +
                      ".open")
        ->Wait();
  }

  void WaitForChromeVoxHintDialogToClose() {
    test::OobeJS()
        .CreateWaiter(test::GetOobeElementPath({kChromeVoxHintDialog}) +
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
    return welcome_screen()
                   ->GetChromeVoxHintDetectorForTesting()
                   ->idle_detector_
               ? true
               : false;
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
  monitor.ExpectSpeech(kChromeVoxHintLaptopSpokenString);
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
  monitor.ExpectSpeech(kChromeVoxHintLaptopSpokenString);
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
  monitor.ExpectSpeech(
      "Do you want to activate ChromeVox, the built-in screen reader for "
      "ChromeOS? If so, press and hold both volume keys for five seconds.");
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
          "voicesChangedListenerMaybeGiveChromeVoxHint_ !== undefined")
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
  monitor.ExpectSpeech(kChromeVoxHintLaptopSpokenString);
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
        FILE_STARTUP_CUSTOMIZATION_MANIFEST, startup_manifest);
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
  monitor.ExpectSpeechPatternWithLocale("*", "fr");
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
    document.getElementById('connect').DEFAULT_CHROMEVOX_HINT_TIMEOUT_MS_ = 0;
    )";
  test::ExecuteOobeJS(set_default_hint_timeout_ms);
  test::ExecuteOobeJS(set_no_french_voice);
  test::SpeechMonitor monitor;
  test::OobeJS().ExpectAttributeEQ("open", kChromeVoxHintDialog, false);
  GiveChromeVoxHintForTesting();
  // Expect speech in English, even though the system locale is French.
  monitor.ExpectSpeechPatternWithLocale("*", "en-US");
  monitor.Replay();
  WaitForSpokenSuccessMetric();
}

}  // namespace ash
