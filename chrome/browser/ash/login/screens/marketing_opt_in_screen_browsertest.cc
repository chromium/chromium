// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/marketing_opt_in_screen.h"

#include <initializer_list>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/marketing_backend_connector.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/c/system/trap.h"

namespace ash {
namespace {

const test::UIPath kChromebookGameTitle = {"marketing-opt-in",
                                           "marketingOptInGameDeviceTitle"};
const test::UIPath kChromebookGameSubtitle = {
    "marketing-opt-in", "marketingOptInGameDeviceSubtitle"};
const test::UIPath kChromebookEmailToggle = {"marketing-opt-in",
                                             "chromebookUpdatesOption"};
const test::UIPath kChromebookEmailToggleDiv = {"marketing-opt-in",
                                                "toggleRow"};
const test::UIPath kChromebookEmailLegalFooterDiv = {"marketing-opt-in",
                                                     "legalFooter"};
const test::UIPath kMarketingA11yButton = {
    "marketing-opt-in", "marketing-opt-in-accessibility-button"};
const test::UIPath kMarketingFinalA11yPage = {"marketing-opt-in",
                                              "finalAccessibilityPage"};
const test::UIPath kMarketingA11yButtonToggle = {"marketing-opt-in",
                                                 "a11yNavButtonToggle"};

// Parameter to be used in tests.
struct RegionToCodeMap {
  const char* test_name;
  const char* region;
  const char* country_code;
  bool is_default_opt_in;
  bool is_unknown_country;
  bool requires_legal_footer;
};

// Default countries
const RegionToCodeMap kDefaultCountries[]{
    {"US", "America/Los_Angeles", "us", true, false, false},
    {"Canada", "Canada/Atlantic", "ca", false, false, true},
    {"UnitedKingdom", "Europe/London", "gb", false, false, false}};

// Extended region list. Behind feature flag.
const RegionToCodeMap kExtendedCountries[]{
    {"France", "Europe/Paris", "fr", false, false, false},
    {"Netherlands", "Europe/Amsterdam", "nl", false, false, false},
    {"Finland", "Europe/Helsinki", "fi", false, false, false},
    {"Sweden", "Europe/Stockholm", "se", false, false, false},
    {"Norway", "Europe/Oslo", "no", false, false, false},
    {"Denmark", "Europe/Copenhagen", "dk", false, false, false},
    {"Spain", "Europe/Madrid", "es", false, false, false},
    {"Italy", "Europe/Rome", "it", false, false, false},
    {"Japan", "Asia/Tokyo", "jp", false, false, false},
    {"Australia", "Australia/Sydney", "au", false, false, false}};

// Double opt-in countries. Behind double opt-in feature flag.
const RegionToCodeMap kDoubleOptInCountries[]{
    {"Germany", "Europe/Berlin", "de", false, false, false}};

// Base class for simple tests on the marketing opt-in screen.
class MarketingOptInScreenTest : public OobeBaseTest,
                                 public LocalStateMixin::Delegate {
 public:
  ~MarketingOptInScreenTest() override = default;

  // OobeBaseTest:
  void SetUpOnMainThread() override;

  MarketingOptInScreen* GetScreen();
  void ShowMarketingOptInScreen();
  void TapOnGetStartedAndWaitForScreenExit();
  void ShowAccessibilityButtonForTest();
  // Expects that the option to opt-in is not visible.
  void ExpectNoOptInOption();
  // Expects that the option to opt-in is visible.
  void ExpectOptInOptionAvailable();
  // Expects a verbose footer containing legal information.
  void ExpectLegalFooterVisibility(bool visibility);
  // Expects that the opt-in toggle is visible and unchecked.
  void ExpectOptedOut();
  // Expects that the opt-in toggle is visible and checked.
  void ExpectOptedIn();
  void ExpectRecordedUserPrefRegardingChoice(bool opted_in);
  // Flips the toggle to opt-in. Only to be called when the toggle is unchecked.
  void OptIn();
  void OptOut();

  void ExpectGeolocationMetric(bool resolved);
  void WaitForScreenExit();

  // US as default location for non-parameterized tests.
  void SetUpLocalState() override {
    g_browser_process->local_state()->SetString(::prefs::kSigninScreenTimezone,
                                                "America/Los_Angeles");
  }

  // Logs in as a normal user. Overridden by subclasses.
  virtual void PerformLogin();

  std::optional<MarketingOptInScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;

 protected:
  base::test::ScopedFeatureList feature_list_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {}, &fake_gaia_};

 private:
  void HandleScreenExit(MarketingOptInScreen::Result result);

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;
  MarketingOptInScreen::ScreenExitCallback original_callback_;

  FakeGaiaMixin fake_gaia_{&mixin_host_};
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

/**
 * For testing backend requests
 */
class MarketingOptInScreenTestWithRequest : public MarketingOptInScreenTest {
 public:
  MarketingOptInScreenTestWithRequest() = default;
  ~MarketingOptInScreenTestWithRequest() override = default;

  void WaitForBackendRequest();
  void HandleBackendRequest(std::string country_code);
  std::string GetRequestedCountryCode() { return requested_country_code_; }

 private:
  bool backend_request_performed_ = false;
  base::RepeatingClosure backend_request_callback_;
  std::string requested_country_code_;

  ScopedRequestCallbackSetter callback_setter{
      std::make_unique<base::RepeatingCallback<void(std::string)>>(
          base::BindRepeating(
              &MarketingOptInScreenTestWithRequest::HandleBackendRequest,
              base::Unretained(this)))};
};

void MarketingOptInScreenTest::SetUpOnMainThread() {
  ShellTestApi().SetTabletModeEnabledForTest(true);

  original_callback_ = GetScreen()->get_exit_callback_for_testing();
  GetScreen()->set_exit_callback_for_testing(base::BindRepeating(
      &MarketingOptInScreenTest::HandleScreenExit, base::Unretained(this)));
  GetScreen()->set_ingore_pref_sync_for_testing(true);

  OobeBaseTest::SetUpOnMainThread();
  auto* wizard_context = LoginDisplayHost::default_host()->GetWizardContext();
  wizard_context->is_branded_build = true;
  wizard_context->defer_oobe_flow_finished_for_tests = true;
}

MarketingOptInScreen* MarketingOptInScreenTest::GetScreen() {
  return WizardController::default_controller()
      ->GetScreen<MarketingOptInScreen>();
}

void MarketingOptInScreenTest::ShowMarketingOptInScreen() {
  PerformLogin();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  LoginDisplayHost::default_host()->StartWizard(
      MarketingOptInScreenView::kScreenId);
}

void MarketingOptInScreenTest::TapOnGetStartedAndWaitForScreenExit() {
  test::OobeJS().TapOnPath(
      {"marketing-opt-in", "marketing-opt-in-next-button"});
  WaitForScreenExit();
}

void MarketingOptInScreenTest::ShowAccessibilityButtonForTest() {
  GetScreen()->SetA11yButtonVisibilityForTest(true /* shown */);
}

void MarketingOptInScreenTest::ExpectNoOptInOption() {
  test::OobeJS().ExpectVisiblePath(
      {"marketing-opt-in", "marketingOptInOverviewDialog"});
  test::OobeJS().ExpectHiddenPath(kChromebookEmailToggleDiv);
}

void MarketingOptInScreenTest::ExpectOptInOptionAvailable() {
  test::OobeJS().ExpectVisiblePath(
      {"marketing-opt-in", "marketingOptInOverviewDialog"});
  test::OobeJS().ExpectVisiblePath(kChromebookEmailToggleDiv);
}

void MarketingOptInScreenTest::ExpectLegalFooterVisibility(bool visibility) {
  ExpectOptInOptionAvailable();
  if (visibility) {
    test::OobeJS().ExpectVisiblePath(kChromebookEmailLegalFooterDiv);
  } else {
    test::OobeJS().ExpectHiddenPath(kChromebookEmailLegalFooterDiv);
  }
}

void MarketingOptInScreenTest::ExpectOptedOut() {
  ExpectOptInOptionAvailable();
  test::OobeJS().ExpectHasNoAttribute("checked", kChromebookEmailToggle);
}

void MarketingOptInScreenTest::ExpectOptedIn() {
  ExpectOptInOptionAvailable();
  test::OobeJS().ExpectHasAttribute("checked", kChromebookEmailToggle);
}

void MarketingOptInScreenTest::ExpectRecordedUserPrefRegardingChoice(
    bool opted_in) {
  EXPECT_TRUE(
      ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetUserPrefValue(
          prefs::kOobeMarketingOptInChoice) != nullptr);
  EXPECT_EQ(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
                prefs::kOobeMarketingOptInChoice),
            opted_in);
}

void MarketingOptInScreenTest::OptIn() {
  ExpectOptedOut();
  test::OobeJS().ClickOnPath(kChromebookEmailToggle);
  test::OobeJS().ExpectHasAttribute("checked", kChromebookEmailToggle);
}

void MarketingOptInScreenTest::OptOut() {
  ExpectOptedIn();
  test::OobeJS().ClickOnPath(kChromebookEmailToggle);
  test::OobeJS().ExpectHasNoAttribute("checked", kChromebookEmailToggle);
}

void MarketingOptInScreenTest::ExpectGeolocationMetric(bool resolved) {
  histogram_tester_.ExpectUniqueSample(
      "OOBE.MarketingOptInScreen.GeolocationResolve",
      resolved
          ? MarketingOptInScreen::GeolocationEvent::
                kCountrySuccessfullyDetermined
          : MarketingOptInScreen::GeolocationEvent::kCouldNotDetermineCountry,
      1);
}

void MarketingOptInScreenTest::WaitForScreenExit() {
  if (screen_exited_)
    return;

  base::RunLoop run_loop;
  screen_exit_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MarketingOptInScreenTest::PerformLogin() {
  login_manager_mixin_.LoginAsNewRegularUser();
}

void MarketingOptInScreenTest::HandleScreenExit(
    MarketingOptInScreen::Result result) {
  ASSERT_FALSE(screen_exited_);
  screen_exited_ = true;
  screen_result_ = result;
  original_callback_.Run(result);
  if (screen_exit_callback_)
    std::move(screen_exit_callback_).Run();
}

void MarketingOptInScreenTestWithRequest::WaitForBackendRequest() {
  if (backend_request_performed_)
    return;
  base::RunLoop run_loop;
  backend_request_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MarketingOptInScreenTestWithRequest::HandleBackendRequest(
    std::string country_code) {
  ASSERT_FALSE(backend_request_performed_);
  backend_request_performed_ = true;
  requested_country_code_ = country_code;
  if (backend_request_callback_)
    std::move(backend_request_callback_).Run();
}

// Tests that the screen is visible
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, ScreenVisible) {
  PerformLogin();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  // Expect the screen to not have been shown before.
  EXPECT_FALSE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kOobeMarketingOptInScreenFinished));
  LoginDisplayHost::default_host()->StartWizard(
      MarketingOptInScreenView::kScreenId);

  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(
      {"marketing-opt-in", "marketingOptInOverviewDialog"});
  TapOnGetStartedAndWaitForScreenExit();

  // Expect the screen to be marked as shown.
  EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kOobeMarketingOptInScreenFinished));
}

IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, OptInFlow) {
  ShowMarketingOptInScreen();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();
  // U.S. is the default region for the base tests.
  ExpectOptedIn();
  TapOnGetStartedAndWaitForScreenExit();

  // Expect the user preference to have been stored as opted-in (true).
  ExpectRecordedUserPrefRegardingChoice(true);
}

IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, OptOutFlow) {
  ShowMarketingOptInScreen();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();
  // U.S. is the default region for the base tests.
  ExpectOptedIn();
  OptOut();
  TapOnGetStartedAndWaitForScreenExit();

  // Expect the user preference to have been stored as opted-out (false).
  ExpectRecordedUserPrefRegardingChoice(false);
}

// Tests that the option to sign up for emails isn't shown when the user
// already made its choice.
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, HideOptionWhenChoiceKnown) {
  PerformLogin();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  // Mark the screen as shown before and the user's choice as 'not opted in'.
  ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetBoolean(
      prefs::kOobeMarketingOptInScreenFinished, true);
  ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetBoolean(
      prefs::kOobeMarketingOptInChoice, false);

  LoginDisplayHost::default_host()->StartWizard(
      MarketingOptInScreenView::kScreenId);

  ExpectNoOptInOption();
  TapOnGetStartedAndWaitForScreenExit();
}

// Tests that the option to sign up is shown if the screen was shown before
// but the user did not have an option to sign up for emails. (No user
// preference stored)
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest,
                       ShowOptionWhenNoChoiceOnRecord) {
  PerformLogin();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetBoolean(
      prefs::kOobeMarketingOptInScreenFinished, true);

  LoginDisplayHost::default_host()->StartWizard(
      MarketingOptInScreenView::kScreenId);

  ExpectOptInOptionAvailable();
  TapOnGetStartedAndWaitForScreenExit();

  // Expect the user preference to have been stored as opted-in (true).
  ExpectRecordedUserPrefRegardingChoice(true);
}

// Tests that the user can enable shelf navigation buttons in tablet mode from
// the screen.
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, EnableShelfNavigationButtons) {
  ShowMarketingOptInScreen();
  ShowAccessibilityButtonForTest();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();

  // Tap on accessibility settings link, and wait for the accessibility settings
  // UI to show up.
  test::OobeJS().CreateVisibilityWaiter(true, kMarketingA11yButton)->Wait();
  test::OobeJS().ClickOnPath(kMarketingA11yButton);
  test::OobeJS().CreateVisibilityWaiter(true, kMarketingFinalA11yPage)->Wait();

  // Tap the shelf navigation buttons in tablet mode toggle.
  test::OobeJS()
      .CreateVisibilityWaiter(true, kMarketingA11yButtonToggle)
      ->Wait();
  test::OobeJS().ClickOnPath(
      {"marketing-opt-in", "a11yNavButtonToggle", "button"});

  // Go back to the first screen.
  test::OobeJS().TapOnPath(
      {"marketing-opt-in", "final-accessibility-back-button"});

  test::OobeJS()
      .CreateVisibilityWaiter(
          true, {"marketing-opt-in", "marketingOptInOverviewDialog"})
      ->Wait();

  TapOnGetStartedAndWaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), MarketingOptInScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Marketing-opt-in.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Marketing-opt-in",
                                     1);

  // Verify the accessibility pref for shelf navigation buttons is set.
  EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled));
}

// Tests that the user can exit the screen from the accessibility page.
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, ExitScreenFromA11yPage) {
  ShowMarketingOptInScreen();
  ShowAccessibilityButtonForTest();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();

  // Tap on accessibility settings link, and wait for the accessibility settings
  // UI to show up.
  test::OobeJS().CreateVisibilityWaiter(true, kMarketingA11yButton)->Wait();
  test::OobeJS().ClickOnPath(kMarketingA11yButton);
  test::OobeJS().CreateVisibilityWaiter(true, kMarketingFinalA11yPage)->Wait();

  // Tapping the next button exits the screen.
  test::OobeJS().TapOnPath(
      {"marketing-opt-in", "final-accessibility-next-button"});
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), MarketingOptInScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Marketing-opt-in.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Marketing-opt-in",
                                     1);
}

// Interface for setting up parameterized tests based on the region.
class RegionAsParameterInterface
    : public ::testing::WithParamInterface<RegionToCodeMap> {
 public:
  static std::string ParamInfoToString(
      ::testing::TestParamInfo<RegionToCodeMap> param_info) {
    return param_info.param.test_name;
  }

  void SetUpLocalStateRegion() {
    RegionToCodeMap param = GetParam();
    g_browser_process->local_state()->SetString(::prefs::kSigninScreenTimezone,
                                                param.region);
  }
};

// Tests that all country codes are correct given the timezone.
class MarketingTestCountryCodes : public MarketingOptInScreenTestWithRequest,
                                  public RegionAsParameterInterface {
 public:
  MarketingTestCountryCodes() = default;
  ~MarketingTestCountryCodes() = default;

  void SetUpLocalState() override { SetUpLocalStateRegion(); }
};

// Tests that the given timezone resolves to the correct location and
// generates a request for the server with the correct region code.
IN_PROC_BROWSER_TEST_P(MarketingTestCountryCodes, CountryCodes) {
  const RegionToCodeMap param = GetParam();
  ShowMarketingOptInScreen();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();

  ExpectLegalFooterVisibility(param.requires_legal_footer);
  if (param.is_default_opt_in) {
    ExpectOptedIn();
  } else {
    ExpectOptedOut();
    OptIn();
  }

  TapOnGetStartedAndWaitForScreenExit();
  WaitForBackendRequest();
  EXPECT_EQ(GetRequestedCountryCode(), param.country_code);
  const auto event =
      (param.is_default_opt_in)
          ? MarketingOptInScreen::Event::kUserOptedInWhenDefaultIsOptIn
          : MarketingOptInScreen::Event::kUserOptedInWhenDefaultIsOptOut;
  histogram_tester_.ExpectUniqueSample(
      "OOBE.MarketingOptInScreen.Event." + std::string(param.country_code),
      event, 1);
  // Expect a generic event in addition to the country specific one.
  histogram_tester_.ExpectUniqueSample("OOBE.MarketingOptInScreen.Event", event,
                                       1);

  // Expect successful geolocation resolve.
  ExpectGeolocationMetric(/*resolved=*/true);
}

// Test all the countries lists.
INSTANTIATE_TEST_SUITE_P(MarketingOptInDefaultCountries,
                         MarketingTestCountryCodes,
                         testing::ValuesIn(kDefaultCountries),
                         RegionAsParameterInterface::ParamInfoToString);
INSTANTIATE_TEST_SUITE_P(MarketingOptInExtendedCountries,
                         MarketingTestCountryCodes,
                         testing::ValuesIn(kExtendedCountries),
                         RegionAsParameterInterface::ParamInfoToString);
INSTANTIATE_TEST_SUITE_P(MarketingOptInDoubleOptInCountries,
                         MarketingTestCountryCodes,
                         testing::ValuesIn(kDoubleOptInCountries),
                         RegionAsParameterInterface::ParamInfoToString);

class MarketingOptInScreenTestChildUser : public MarketingOptInScreenTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    // Child users require a user policy, set up an empty one so the user can
    // get through login.
    ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
    MarketingOptInScreenTest::SetUpInProcessBrowserTestFixture();
  }
  void PerformLogin() override { login_manager_mixin_.LoginAsNewChildUser(); }

 private:
  EmbeddedPolicyTestServerMixin policy_server_mixin_{&mixin_host_};
  UserPolicyMixin user_policy_mixin_{
      &mixin_host_,
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId),
      &policy_server_mixin_};
};

IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTestChildUser, DisabledForChild) {
  ShowMarketingOptInScreen();
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(),
            MarketingOptInScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Marketing-opt-in.Next", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Marketing-opt-in",
                                     0);
}

class MarketingOptInScreenTestNotBrandedChrome
    : public MarketingOptInScreenTest {
 protected:
  void SetUpOnMainThread() override {
    MarketingOptInScreenTest::SetUpOnMainThread();
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        false;
  }
};

IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTestNotBrandedChrome,
                       SkippedNotBrandedBuild) {
  ShowMarketingOptInScreen();
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(),
            MarketingOptInScreen::Result::NOT_APPLICABLE);
}

class MarketingOptInScreenTestGameDevice : public MarketingOptInScreenTest {
 public:
  MarketingOptInScreenTestGameDevice() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        {
            chromeos::features::kCloudGamingDevice,
        },
        {});
  }
};

IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTestGameDevice,
                       ScreenElementsVisible) {
  PerformLogin();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  // Expect the screen to not have been shown before.
  EXPECT_FALSE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kOobeMarketingOptInScreenFinished));
  LoginDisplayHost::default_host()->StartWizard(
      MarketingOptInScreenView::kScreenId);

  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();
  // check the Screen is Visible
  test::OobeJS().ExpectVisiblePath(
      {"marketing-opt-in", "marketingOptInOverviewDialog"});
  // check the correct game mode title is visible
  test::OobeJS().ExpectVisiblePath(kChromebookGameTitle);
  // check the correct game mode description is visible
  test::OobeJS().ExpectVisiblePath(kChromebookGameSubtitle);
  TapOnGetStartedAndWaitForScreenExit();

  // Expect the screen to be marked as shown.
  EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kOobeMarketingOptInScreenFinished));
}

}  // namespace
}  // namespace ash
