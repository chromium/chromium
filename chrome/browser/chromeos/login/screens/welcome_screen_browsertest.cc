// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

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
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_switches.h"
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

chromeos::OobeUI* GetOobeUI() {
  auto* host = chromeos::LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

void ToggleAccessibilityFeature(const std::string& feature_name,
                                bool new_value) {
  test::JSChecker js = test::OobeJS();
  std::string feature_toggle =
      test::GetOobeElementPath({"connect", feature_name, "button"}) +
      ".checked";
  js.set_polymer_ui(false);

  if (!new_value)
    feature_toggle = "!" + feature_toggle;

  js.ExpectVisiblePath({"connect", feature_name, "button"});
  EXPECT_FALSE(js.GetBool(feature_toggle));
  js.TapOnPath({"connect", feature_name, "button"});
  js.CreateWaiter(feature_toggle)->Wait();
}

class LanguageReloadObserver : public WelcomeScreen::Observer {
 public:
  explicit LanguageReloadObserver(WelcomeScreen* welcome_screen)
      : welcome_screen_(welcome_screen) {
    welcome_screen_->AddObserver(this);
  }

  // WelcomeScreen::Observer:
  void OnLanguageListReloaded() override { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

  ~LanguageReloadObserver() override { welcome_screen_->RemoveObserver(this); }

 private:
  WelcomeScreen* const welcome_screen_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(LanguageReloadObserver);
};

}  // namespace

class WelcomeScreenBrowserTest : public InProcessBrowserTest {
 public:
  WelcomeScreenBrowserTest() = default;
  ~WelcomeScreenBrowserTest() override = default;

  // InProcessBrowserTest:

  void SetUpOnMainThread() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    base::FilePath startup_manifest =
        data_dir_.GetPath().AppendASCII("startup_manifest.json");
    base::WriteFile(startup_manifest, kStartupManifest,
                    strlen(kStartupManifest));
    path_override_ = std::make_unique<base::ScopedPathOverride>(
        chromeos::FILE_STARTUP_CUSTOMIZATION_MANIFEST, startup_manifest);

    ShowLoginWizard(OobeScreen::SCREEN_TEST_NO_WINDOW);
    test::TestPredicateWaiter(base::BindRepeating([]() {
      return WizardController::default_controller() != nullptr;
    })).Wait();
    WizardController::default_controller()
        ->screen_manager()
        ->DeleteScreenForTesting(WelcomeView::kScreenId);
    auto welcome_screen = std::make_unique<WelcomeScreen>(
        GetOobeUI()->GetView<WelcomeScreenHandler>(),
        base::BindRepeating(&WelcomeScreenBrowserTest::OnWelcomeScreenExit,
                            base::Unretained(this)));
    welcome_screen_ = welcome_screen.get();
    observer_ = std::make_unique<LanguageReloadObserver>(welcome_screen_);
    WizardController::default_controller()
        ->screen_manager()
        ->SetScreenForTesting(std::move(welcome_screen));
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    observer_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void WaitForScreenExit() {
    if (screen_exit_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnWelcomeScreenExit() {
    screen_exit_ = true;
    if (screen_exit_callback_) {
      std::move(screen_exit_callback_).Run();
    }
  }

  WelcomeScreen* welcome_screen_ = nullptr;
  std::unique_ptr<LanguageReloadObserver> observer_;

 private:
  std::unique_ptr<base::ScopedPathOverride> path_override_;
  base::ScopedTempDir data_dir_;
  bool screen_exit_ = false;

  base::OnceClosure screen_exit_callback_;
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

class WelcomeScreenWithExperimentalAccessibilityFeaturesTest
    : public WelcomeScreenBrowserTest {
 public:
  WelcomeScreenWithExperimentalAccessibilityFeaturesTest() = default;
  ~WelcomeScreenWithExperimentalAccessibilityFeaturesTest() override = default;

  // WelcomeScreenBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilityFeatures);
    WelcomeScreenBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, WelcomeScreenElements) {
  welcome_screen_->Show();
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
  test::OobeJS().ExpectVisiblePath(
      {"connect", "welcomeScreen", "enableDebuggingLink"});
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, WelcomeScreenNext) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "welcomeNextButton"});
  WaitForScreenExit();
}

// Set of browser tests for Welcome Screen Language options.
IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, WelcomeScreenLanguageFlow) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "languageSelectionButton"});

  test::OobeJS().TapOnPath({"connect", "ok-button-language"});
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "welcomeNextButton"});
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenLanguageElements) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "languageSelectionButton"});

  test::OobeJS().ExpectVisiblePath({"connect", "languageDropdownContainer"});
  test::OobeJS().ExpectVisiblePath({"connect", "keyboardDropdownContainer"});
  test::OobeJS().ExpectVisiblePath({"connect", "languageSelect"});
  test::OobeJS().ExpectVisiblePath({"connect", "keyboardSelect"});
}

// Flaky: https://crbug.com/1025396.
IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       DISABLED_WelcomeScreenLanguageSelection) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "languageSelectionButton"});
  ASSERT_TRUE(g_browser_process->GetApplicationLocale() == "en-US");
  test::OobeJS().GetBool(
      "document.getElementById('connect').$.welcomeScreen.currentLanguage == "
      "'English (United States)'");

  test::OobeJS().SelectElementInPath("fr",
                                     {"connect", "languageSelect", "select"});
  test::OobeJS().GetBool(
      "document.getElementById('connect').$.welcomeScreen.currentLanguage == "
      "'français'");
  ASSERT_TRUE(g_browser_process->GetApplicationLocale() == "fr");

  test::OobeJS().SelectElementInPath("en-US",
                                     {"connect", "languageSelect", "select"});
  test::OobeJS().GetBool(
      "document.getElementById('connect').$.welcomeScreen.currentLanguage == "
      "'English (United States)'");
  ASSERT_TRUE(g_browser_process->GetApplicationLocale() == "en-US");
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenKeyboardSelection) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "languageSelectionButton"});

  std::string extension_id_prefix =
      std::string("_comp_ime_") + extension_ime_util::kXkbExtensionId;

  test::OobeJS().SelectElementInPath(extension_id_prefix + "xkb:us:intl:eng",
                                     {"connect", "keyboardSelect", "select"});
  test::OobeJS().GetBool(
      "document.getElementById('connect').$.welcomeScreen.currentKeyboard=="
      "'US'");
  ASSERT_TRUE(welcome_screen_->GetInputMethod() ==
              extension_id_prefix + "xkb:us:intl:eng");

  test::OobeJS().SelectElementInPath(extension_id_prefix + "xkb:us:workman:eng",
                                     {"connect", "keyboardSelect", "select"});
  test::OobeJS().GetBool(
      std::string(
          "document.getElementById('connect').$.welcomeScreen.currentKeyboard=="
          "'") +
      extension_id_prefix + "xkb:us:workman:eng'");
  ASSERT_TRUE(welcome_screen_->GetInputMethod() ==
              extension_id_prefix + "xkb:us:workman:eng");
}

// Set of browser tests for Welcome Screen Accessibility options.
IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilityFlow) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  test::OobeJS().TapOnPath({"connect", "ok-button-accessibility"});
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "welcomeNextButton"});
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilitySpokenFeedback) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  ToggleAccessibilityFeature("accessibility-spoken-feedback", true);
  ASSERT_TRUE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  ToggleAccessibilityFeature("accessibility-spoken-feedback", false);
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilityLargeCursor) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(AccessibilityManager::Get()->IsLargeCursorEnabled());
  ToggleAccessibilityFeature("accessibility-large-cursor", true);
  ASSERT_TRUE(AccessibilityManager::Get()->IsLargeCursorEnabled());

  ToggleAccessibilityFeature("accessibility-large-cursor", false);
  ASSERT_FALSE(AccessibilityManager::Get()->IsLargeCursorEnabled());
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilityHighContrast) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(AccessibilityManager::Get()->IsHighContrastEnabled());
  ToggleAccessibilityFeature("accessibility-high-contrast", true);
  ASSERT_TRUE(AccessibilityManager::Get()->IsHighContrastEnabled());

  ToggleAccessibilityFeature("accessibility-high-contrast", false);
  ASSERT_FALSE(AccessibilityManager::Get()->IsHighContrastEnabled());
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilitySelectToSpeak) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());
  ToggleAccessibilityFeature("accessibility-select-to-speak", true);
  ASSERT_TRUE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());

  ToggleAccessibilityFeature("accessibility-select-to-speak", false);
  ASSERT_FALSE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest,
                       WelcomeScreenAccessibilityScreenMagnifier) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(MagnificationManager::Get()->IsMagnifierEnabled());
  ToggleAccessibilityFeature("accessibility-screen-magnifier", true);
  ASSERT_TRUE(MagnificationManager::Get()->IsMagnifierEnabled());

  ToggleAccessibilityFeature("accessibility-screen-magnifier", false);
  ASSERT_FALSE(MagnificationManager::Get()->IsMagnifierEnabled());
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, A11yDockedMagnifierDisabled) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath({"connect", "dockedMagnifierOobeOption"});
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, PRE_SelectedLanguage) {
  EXPECT_EQ(
      StartupCustomizationDocument::GetInstance()->initial_locale_default(),
      "en-US");
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  const std::string locale = "ru";
  welcome_screen_->SetApplicationLocale(locale);
  test::OobeJS().TapOnPath({"connect", "welcomeScreen", "welcomeNextButton"});
  WaitForScreenExit();
  EXPECT_EQ(g_browser_process->local_state()->GetString(
                language::prefs::kApplicationLocale),
            locale);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, SelectedLanguage) {
  observer_->Wait();
  const std::string locale = "ru";
  EXPECT_EQ(g_browser_process->local_state()->GetString(
                language::prefs::kApplicationLocale),
            locale);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenWithExperimentalAccessibilityFeaturesTest,
                       A11yDockedMagnifierEnabled) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(MagnificationManager::Get()->IsDockedMagnifierEnabled());
  ToggleAccessibilityFeature("dockedMagnifierOobeOption", true);
  ASSERT_TRUE(MagnificationManager::Get()->IsDockedMagnifierEnabled());

  ToggleAccessibilityFeature("dockedMagnifierOobeOption", false);
  ASSERT_FALSE(MagnificationManager::Get()->IsDockedMagnifierEnabled());
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenBrowserTest, A11yVirtualKeyboard) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().TapOnPath(
      {"connect", "welcomeScreen", "accessibilitySettingsButton"});

  ASSERT_FALSE(AccessibilityManager::Get()->IsVirtualKeyboardEnabled());
  ToggleAccessibilityFeature("accessibility-virtual-keyboard", true);
  ASSERT_TRUE(AccessibilityManager::Get()->IsVirtualKeyboardEnabled());

  ToggleAccessibilityFeature("accessibility-virtual-keyboard", false);
  ASSERT_FALSE(AccessibilityManager::Get()->IsVirtualKeyboardEnabled());
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenSystemDevModeBrowserTest,
                       DebuggerModeTest) {
  welcome_screen_->Show();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().ClickOnPath(
      {"connect", "welcomeScreen", "enableDebuggingLink"});

  test::OobeJS().ExpectVisiblePath({"debugging-remove-protection-button"});
  test::OobeJS().ExpectVisiblePath({"debugging-cancel-button"});
  test::OobeJS().ExpectVisiblePath({"enable-debugging-help-link"});
  test::OobeJS().ClickOnPath({"debugging-cancel-button"});
}

}  // namespace chromeos
