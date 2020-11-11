// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/screens/welcome_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screens_utils.h"
#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"

namespace chromeos {

namespace {

const char kStartupManifest[] =
    R"({
      "version": "1.0",
      "initial_locale" : "en-US",
      "initial_timezone" : "US/Pacific",
      "keyboard_layout" : "xkb:us::eng",
    })";

const char kCurrentLang[] =
    R"(document.getElementById('connect').$.welcomeScreen.currentLanguage)";
const char kCurrentKeyboard[] =
    R"(document.getElementById('connect').currentKeyboard)";

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
    EXPECT_TRUE(base::WriteFile(startup_manifest, kStartupManifest));
    path_override_ = std::make_unique<base::ScopedPathOverride>(
        chromeos::FILE_STARTUP_CUSTOMIZATION_MANIFEST, startup_manifest);
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

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, WelcomeScreenElements) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  test::OobeJS().ExpectVisiblePath({"connect", "welcomeScreen"});
  test::OobeJS().ExpectHiddenPath({"connect", "accessibilityScreen"});
  test::OobeJS().ExpectHiddenPath({"connect", "languageScreen"});
  test::OobeJS().ExpectHiddenPath({"connect", "timezoneScreen"});
  test::OobeJS().ExpectVisiblePath(
      {"connect", "welcomeScreen", "welcomeNextButton"});
  test::OobeJS().ExpectVisiblePath(
      {"connect", "welcomeScreen", "languageSelectionButton"});
  test::OobeJS().ExpectVisiblePath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});
  test::OobeJS().ExpectHiddenPath(
      {"connect", "welcomeScreen", "timezoneSettingsButton"});
  test::OobeJS().ExpectVisiblePath(
      {"connect", "welcomeScreen", "enableDebuggingLink"});
}

// This is a minimal possible test for OOBE. It is used as reference test
// for measurements during OOBE speedup work.
// TODO(crbug.com/1058022): Remove after speedup work.
IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, OobeStartupTime) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, WelcomeScreenNext) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "welcomeNextButton"});
  WaitForScreenExit();
}

// Set of browser tests for Welcome Screen Language options.
IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, WelcomeScreenLanguageFlow) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "languageSelectionButton"});

  test::OobeJS().TapOnPath({"connect", "ok-button-language"});
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "welcomeNextButton"});
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenLanguageElements) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "languageSelectionButton"});

  test::OobeJS().ExpectVisiblePath({"connect", "languageDropdownContainer"});
  test::OobeJS().ExpectVisiblePath({"connect", "keyboardDropdownContainer"});
  test::OobeJS().ExpectVisiblePath({"connect", "languageSelect"});
  test::OobeJS().ExpectVisiblePath({"connect", "keyboardSelect"});
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenLanguageSelection) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

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
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
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
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  test::OobeJS().TapOnPath({"connect", "ok-button-accessibility"});
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "welcomeNextButton"});
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilitySpokenFeedback) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
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
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
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
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
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
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
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
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
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
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
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
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  const std::string locale = "ru";
  welcome_screen()->SetApplicationLocale(locale);
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "welcomeNextButton"});
  WaitForScreenExit();
  EXPECT_EQ(g_browser_process->local_state()->GetString(
                language::prefs::kApplicationLocale),
            locale);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, SelectedLanguage) {
  const std::string locale = "ru";
  EXPECT_EQ(g_browser_process->local_state()->GetString(
                language::prefs::kApplicationLocale),
            locale);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, A11yVirtualKeyboard) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
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

IN_PROC_BROWSER_TEST_F(WelcomeScreenSystemDevModeBrowserTest,
                       DebuggerModeTest) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().ClickOnPath(
      {"connect", "welcomeScreen", "enableDebuggingLink"});

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
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
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
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "welcomeNextButton"});
  WaitForScreenExit();

  // Must not change.
  CheckTimezone(kTestTimezone);
}

}  // namespace chromeos
