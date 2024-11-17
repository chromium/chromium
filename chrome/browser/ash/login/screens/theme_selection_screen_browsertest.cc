// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/login/screens/theme_selection_screen.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/screens/guest_tos_screen.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/fake_eula_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/guest_tos_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

using ::testing::ElementsAre;

constexpr char kThemeSelectionId[] = "theme-selection";
constexpr char kLightThemeButton[] = "lightThemeButton";
constexpr char kDarkThemeButton[] = "darkThemeButton";
constexpr char kAutoThemeButton[] = "autoThemeButton";
const test::UIPath kDarkThemeButtonPath = {kThemeSelectionId, kDarkThemeButton};
const test::UIPath kLightThemeButtonPath = {kThemeSelectionId,
                                            kLightThemeButton};
const test::UIPath kAutoThemeButtonPath = {kThemeSelectionId, kAutoThemeButton};
const test::UIPath kNextButtonPath = {kThemeSelectionId, "nextButton"};
const test::UIPath kScreenSubtitleClamshellPath = {
    kThemeSelectionId, "theme-selection-subtitle-clamshell"};
const test::UIPath kScreenSubtitleTabletPath = {
    kThemeSelectionId, "theme-selection-subtitle-tablet"};

constexpr char kStepShownStatusHistogram[] =
    "OOBE.StepShownStatus.Theme-selection";
constexpr char kStepCompletionTimeHistogram[] =
    "OOBE.StepCompletionTime.Theme-selection";
constexpr char kProceedExitReasonHistogram[] =
    "OOBE.StepCompletionTimeByExitReason.Theme-selection.Proceed";
constexpr char kSelectedThemeHistogram[] =
    "OOBE.ThemeSelectionScreen.SelectedTheme";

}  // namespace

class ThemeSelectionScreenTest
    : public OobeBaseTest,
      public ::testing::WithParamInterface<std::tuple<test::UIPath, bool>> {
 public:
  void SetUpOnMainThread() override {
    ThemeSelectionScreen* theme_selection_screen =
        WizardController::default_controller()
            ->GetScreen<ThemeSelectionScreen>();

    original_callback_ =
        theme_selection_screen->get_exit_callback_for_testing();
    theme_selection_screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowThemeSelectionScreen() {
    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->skip_choobe_for_tests = true;

    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    WizardController::default_controller()->AdvanceToScreen(
        ThemeSelectionScreenView::kScreenId);
  }

  ThemeSelectionScreen::Result WaitForScreenExitResult() {
    auto result = screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

  void setTabletMode(bool enabled) {
    TabletMode::Waiter waiter(enabled);
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enabled);
    waiter.Wait();
  }

  base::HistogramTester histogram_tester_;

 private:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  ThemeSelectionScreen::ScreenExitCallback original_callback_;
  base::test::TestFuture<ThemeSelectionScreen::Result> screen_result_waiter_;
};

IN_PROC_BROWSER_TEST_F(ThemeSelectionScreenTest,
                       ProceedWithDefaultThemeBrandedBuild) {
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;
  ShowThemeSelectionScreen();
  test::OobeJS().ClickOnPath(kNextButtonPath);

  EXPECT_EQ(WaitForScreenExitResult(), ThemeSelectionScreen::Result::kProceed);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kStepShownStatusHistogram),
      ElementsAre(base::Bucket(
          static_cast<int>(OobeMetricsHelper::ScreenShownStatus::kShown), 1)));
  histogram_tester_.ExpectTotalCount(kStepCompletionTimeHistogram, 1);
  histogram_tester_.ExpectTotalCount(kProceedExitReasonHistogram, 1);
}

IN_PROC_BROWSER_TEST_F(ThemeSelectionScreenTest,
                       ProceedWithDefaultThemeNotBrandedBuild) {
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
      false;
  ShowThemeSelectionScreen();

  test::OobeJS().ClickOnPath(kNextButtonPath);
  EXPECT_EQ(WaitForScreenExitResult(), ThemeSelectionScreen::Result::kProceed);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kStepShownStatusHistogram),
      ElementsAre(base::Bucket(
          static_cast<int>(OobeMetricsHelper::ScreenShownStatus::kShown), 1)));
  histogram_tester_.ExpectTotalCount(kStepCompletionTimeHistogram, 1);
  histogram_tester_.ExpectTotalCount(kProceedExitReasonHistogram, 1);
}

IN_PROC_BROWSER_TEST_P(ThemeSelectionScreenTest, SelectTheme) {
  auto selected_option_path = std::get<0>(GetParam());
  bool branded_build = std::get<1>(GetParam());

  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
      branded_build;

  ShowThemeSelectionScreen();
  Profile* profile = ProfileManager::GetActiveUserProfile();

  // Expect the default dark mode schedule type to be sunset-to-sunrise.
  EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 1);

  test::OobeJS().ExpectVisiblePath(selected_option_path);
  test::OobeJS().ClickOnPath(selected_option_path);

  auto selectedOption =
      selected_option_path.begin()[selected_option_path.size() - 1];
  ThemeSelectionScreen::SelectedTheme theme =
      ThemeSelectionScreen::SelectedTheme::kDark;
  if (selectedOption == kDarkThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled), true);
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 0);
    EXPECT_TRUE(DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());
  } else if (selectedOption == kLightThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled), false);
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 0);
    EXPECT_FALSE(DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());
    theme = ThemeSelectionScreen::SelectedTheme::kLight;
  } else if (selectedOption == kAutoThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 1);
    theme = ThemeSelectionScreen::SelectedTheme::kAuto;
  }

  test::OobeJS().ClickOnPath(kNextButtonPath);

  EXPECT_THAT(histogram_tester_.GetAllSamples(kSelectedThemeHistogram),
              ElementsAre(base::Bucket(static_cast<int>(theme), 1)));
  EXPECT_EQ(WaitForScreenExitResult(), ThemeSelectionScreen::Result::kProceed);
}

IN_PROC_BROWSER_TEST_F(ThemeSelectionScreenTest, ToggleTabletMode) {
  ShowThemeSelectionScreen();
  // by default clamshell mode is used
  test::OobeJS().ExpectVisiblePath(kScreenSubtitleClamshellPath);

  // switch to tablet mode
  setTabletMode(true);
  test::OobeJS().ExpectVisiblePath(kScreenSubtitleTabletPath);

  // and back to clamshell
  setTabletMode(false);
  test::OobeJS().ExpectVisiblePath(kScreenSubtitleClamshellPath);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ThemeSelectionScreenTest,
    testing::Combine(testing::ValuesIn({kDarkThemeButtonPath,
                                        kLightThemeButtonPath,
                                        kAutoThemeButtonPath}),
                     testing::Bool()));

class ThemeSelectionScreenResumeTest
    : public OobeBaseTest,
      public ::testing::WithParamInterface<test::UIPath> {
 protected:
  void SetUpOnMainThread() override {
    cryptohome_mixin_.ApplyAuthConfigIfUserExists(
        user_, test::UserAuthConfig::Create(test::kDefaultAuthSetup));
    OobeBaseTest::SetUpOnMainThread();
  }

  DeviceStateMixin device_state_{
      &mixin_host_,
      DeviceStateMixin::State::OOBE_COMPLETED_PERMANENTLY_UNOWNED};
  FakeGaiaMixin gaia_mixin_{&mixin_host_};
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  LoginManagerMixin login_mixin_{&mixin_host_, LoginManagerMixin::UserList(),
                                 &gaia_mixin_};
  AccountId user_{
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId)};
};

IN_PROC_BROWSER_TEST_P(ThemeSelectionScreenResumeTest, PRE_ResumedScreen) {
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->skip_choobe_for_tests = true;

  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  LoginManagerMixin::TestUserInfo test_user(user_);
  login_mixin_.LoginWithDefaultContext(test_user);
  OobeScreenExitWaiter(UserCreationView::kScreenId).Wait();
  WizardController::default_controller()->AdvanceToScreen(
      ThemeSelectionScreenView::kScreenId);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 1);

  test::OobeJS().ExpectVisiblePath(GetParam());
  test::OobeJS().ClickOnPath(GetParam());

  auto selectedOption = GetParam().begin()[GetParam().size() - 1];
  if (selectedOption == kDarkThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled), true);
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 0);
    EXPECT_TRUE(DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());

  } else if (selectedOption == kLightThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled), false);
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 0);
    EXPECT_FALSE(DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());

  } else if (selectedOption == kAutoThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 1);
  }

  OobeScreenWaiter(ThemeSelectionScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_P(ThemeSelectionScreenResumeTest, ResumedScreen) {
  login_mixin_.LoginAsNewRegularUser();
  OobeScreenWaiter(ThemeSelectionScreenView::kScreenId).Wait();
  test::OobeJS().ExpectHasAttribute("checked", GetParam());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ThemeSelectionScreenResumeTest,
                         ::testing::Values(kDarkThemeButtonPath,
                                           kLightThemeButtonPath,
                                           kAutoThemeButtonPath));

}  // namespace ash
