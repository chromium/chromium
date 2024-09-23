// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/recommend_apps_screen.h"

#include <memory>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher_delegate.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/scoped_test_recommend_apps_fetcher_factory.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/recommend_apps_screen_handler.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

constexpr char kRecommendAppsId[] = "recommend-apps";

const test::UIPath kInstallButton = {kRecommendAppsId, "installButton"};
const test::UIPath kSkipButton = {kRecommendAppsId, "skipButton"};

const test::UIPath kAppsList = {kRecommendAppsId, "appsList"};
const test::UIPath kFirstAppCheckbox = {kRecommendAppsId, "appsList",
                                        R"(test\\.app\\.foo\\.app1)"};
const test::UIPath kSecondAppCheckbox = {kRecommendAppsId, "appsList",
                                         R"(test\\.app\\.foo\\.app2)"};

constexpr char kJsonResponse[] =
    R"json({"recommendedApp": [{
    "androidApp": {
      "packageName": "test.app.foo.app1",
      "title": "Test app 1",
      "icon": {
        "imageUri": "https://play-lh.googleusercontent.com/1IDECLAREATHUMBWAR",
        "dimensions": {
          "width": 512,
          "height": 512
        }
      }
    }
  }, {
    "androidApp": {
      "packageName": "test.app.foo.app2",
      "title": "Test app 2",
      "icon": {
        "imageUri": "https://play-lh.googleusercontent.com/2IDECLAREATHUMBWAR",
        "dimensions": {
          "width": 512,
          "height": 512
        }
      }
    }
  }, {
    "androidApp": {
      "packageName": "test.app.foo.app3",
      "title": "Test app 3",
      "icon": {
        "imageUri": "https://play-lh.googleusercontent.com/3IDECLAREATHUMBWAR",
        "dimensions": {
          "width": 512,
          "height": 512
        }
      }
    }
  }
  ]})json";

class StubRecommendAppsFetcher : public apps::RecommendAppsFetcher {
 public:
  explicit StubRecommendAppsFetcher(
      apps::RecommendAppsFetcherDelegate* delegate)
      : delegate_(delegate) {}
  ~StubRecommendAppsFetcher() override = default;

  bool started() const { return started_; }

  void SimulateSuccess(bool bad_response = false) {
    EXPECT_TRUE(started());
    if (bad_response) {
      delegate_->OnLoadSuccess(base::Value());
      return;
    }
    auto output = base::JSONReader::ReadAndReturnValueWithError(kJsonResponse);
    ASSERT_TRUE(output.has_value());
    delegate_->OnLoadSuccess(std::move(*output));
  }

  void SimulateParseError() {
    EXPECT_TRUE(started_);
    delegate_->OnParseResponseError();
  }

  void SimulateLoadError() {
    EXPECT_TRUE(started_);
    delegate_->OnLoadError();
  }

  // RecommendAppsFetcher:
  void Start() override {
    EXPECT_FALSE(started_);
    started_ = true;
  }
  void Retry() override { NOTREACHED_IN_MIGRATION(); }

 protected:
  const raw_ptr<apps::RecommendAppsFetcherDelegate> delegate_;
  bool started_ = false;
};

class RecommendAppsScreenTest : public OobeBaseTest {
 public:
  RecommendAppsScreenTest() = default;
  ~RecommendAppsScreenTest() override = default;

  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    recommend_apps_fetcher_factory_ =
        std::make_unique<apps::ScopedTestRecommendAppsFetcherFactory>(
            base::BindRepeating(
                &RecommendAppsScreenTest::CreateRecommendAppsFetcher,
                base::Unretained(this)));

    recommend_apps_screen_ = WizardController::default_controller()
                                 ->GetScreen<RecommendAppsScreen>();
    recommend_apps_screen_->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
  }

  void TearDownOnMainThread() override {
    recommend_apps_fetcher_ = nullptr;
    recommend_apps_fetcher_factory_.reset();

    OobeBaseTest::TearDownOnMainThread();
  }

  void ShowRecommendAppsScreen() {
    login_manager_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    LoginDisplayHost::default_host()->StartWizard(
        RecommendAppsScreenView::kScreenId);
  }

  RecommendAppsScreen::Result WaitForScreenExitResult() {
    return screen_result_waiter_.Take();
  }

  void ShowScreenAndExpectLoadingStep() {
    LoginDisplayHost::default_host()
        ->GetWizardContext()
        ->defer_oobe_flow_finished_for_tests = true;
    ShowRecommendAppsScreen();
    OobeScreenWaiter(RecommendAppsScreenView::kScreenId).Wait();
    // Wait for loading screen.
    test::OobeJS()
        .CreateVisibilityWaiter(true, {kRecommendAppsId, "loadingDialog"})
        ->Wait();

    test::OobeJS().ExpectHiddenPath({kRecommendAppsId, "appsDialog"});
  }

  void ExpectAppSelectionStep() {
    test::OobeJS()
        .CreateVisibilityWaiter(true, {kRecommendAppsId, "appsDialog"})
        ->Wait();
    test::OobeJS().ExpectHiddenPath({kRecommendAppsId, "loadingDialog"});
  }

  raw_ptr<RecommendAppsScreen, AcrossTasksDanglingUntriaged>
      recommend_apps_screen_ = nullptr;
  raw_ptr<StubRecommendAppsFetcher, AcrossTasksDanglingUntriaged>
      recommend_apps_fetcher_ = nullptr;

  LoginManagerMixin login_manager_{&mixin_host_};

 private:
  std::unique_ptr<apps::RecommendAppsFetcher> CreateRecommendAppsFetcher(
      apps::RecommendAppsFetcherDelegate* delegate) {
    EXPECT_FALSE(recommend_apps_fetcher_);

    auto fetcher = std::make_unique<StubRecommendAppsFetcher>(delegate);
    recommend_apps_fetcher_ = fetcher.get();
    return fetcher;
  }

  std::unique_ptr<apps::ScopedTestRecommendAppsFetcherFactory>
      recommend_apps_fetcher_factory_;

  base::test::TestFuture<RecommendAppsScreen::Result> screen_result_waiter_;
};

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, BasicSelection) {
  ShowScreenAndExpectLoadingStep();

  recommend_apps_fetcher_->SimulateSuccess();

  ExpectAppSelectionStep();
  test::OobeJS().CreateDisplayedWaiter(true, kAppsList)->Wait();
  test::OobeJS().ExpectPathDisplayed(true, kInstallButton);
  test::OobeJS().ExpectDisabledPath(kInstallButton);
  test::OobeJS().ExpectPathDisplayed(true, kSkipButton);
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().ClickOnPath(kFirstAppCheckbox);
  test::OobeJS().ClickOnPath(kSecondAppCheckbox);

  test::OobeJS().CreateEnabledWaiter(true, kInstallButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().TapOnPath(kInstallButton);

  EXPECT_EQ(WaitForScreenExitResult(), RecommendAppsScreen::Result::kSelected);

  const base::Value::List& fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetList(
          arc::prefs::kArcFastAppReinstallPackages);

  base::Value::List expected_pref_value;
  expected_pref_value.Append("test.app.foo.app1");
  expected_pref_value.Append("test.app.foo.app2");
  EXPECT_EQ(expected_pref_value, fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, SelectionChange) {
  ShowScreenAndExpectLoadingStep();

  recommend_apps_fetcher_->SimulateSuccess();
  ExpectAppSelectionStep();

  test::OobeJS().ExpectPathDisplayed(true, kInstallButton);
  test::OobeJS().ExpectDisabledPath(kInstallButton);
  test::OobeJS().ExpectPathDisplayed(true, kSkipButton);
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().ClickOnPath(kFirstAppCheckbox);
  test::OobeJS().ClickOnPath(kSecondAppCheckbox);

  test::OobeJS().CreateEnabledWaiter(true, kInstallButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().ClickOnPath(kFirstAppCheckbox);

  test::OobeJS().TapOnPath(kInstallButton);

  EXPECT_EQ(WaitForScreenExitResult(), RecommendAppsScreen::Result::kSelected);

  const base::Value::List& fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetList(
          arc::prefs::kArcFastAppReinstallPackages);

  base::Value::List expected_pref_value;
  expected_pref_value.Append("test.app.foo.app2");
  EXPECT_EQ(expected_pref_value, fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, SkipWithSelectedApps) {
  ShowScreenAndExpectLoadingStep();

  recommend_apps_fetcher_->SimulateSuccess();
  ExpectAppSelectionStep();

  test::OobeJS().ExpectDisabledPath(kInstallButton);

  test::OobeJS().ExpectPathDisplayed(true, kInstallButton);
  test::OobeJS().ExpectDisabledPath(kInstallButton);
  test::OobeJS().ExpectPathDisplayed(true, kSkipButton);
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().ClickOnPath(kFirstAppCheckbox);

  test::OobeJS().CreateEnabledWaiter(true, kInstallButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().TapOnPath(kSkipButton);

  EXPECT_EQ(WaitForScreenExitResult(), RecommendAppsScreen::Result::kSkipped);

  const base::Value::List& fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetList(
          arc::prefs::kArcFastAppReinstallPackages);
  EXPECT_EQ(base::Value::List(), fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, SkipWithNoAppsSelected) {
  ShowScreenAndExpectLoadingStep();

  recommend_apps_fetcher_->SimulateSuccess();
  ExpectAppSelectionStep();

  test::OobeJS().ExpectPathDisplayed(true, kInstallButton);
  test::OobeJS().ExpectDisabledPath(kInstallButton);
  test::OobeJS().ExpectPathDisplayed(true, kSkipButton);
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().ClickOnPath(kSecondAppCheckbox);

  test::OobeJS().CreateEnabledWaiter(true, kInstallButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().ClickOnPath(kSecondAppCheckbox);

  test::OobeJS().CreateEnabledWaiter(false, kInstallButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().TapOnPath(kSkipButton);

  EXPECT_EQ(WaitForScreenExitResult(), RecommendAppsScreen::Result::kSkipped);

  const base::Value::List& fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetList(
          arc::prefs::kArcFastAppReinstallPackages);
  EXPECT_EQ(base::Value::List(), fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest,
                       InstallWithNoAppsSelectedDisabled) {
  ShowScreenAndExpectLoadingStep();

  recommend_apps_fetcher_->SimulateSuccess();

  ExpectAppSelectionStep();

  // The install button is expected to be disabled at this point.
  test::OobeJS().ExpectDisabledPath(kInstallButton);
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, NoRecommendedApps) {
  ShowScreenAndExpectLoadingStep();
  recommend_apps_fetcher_->SimulateSuccess(/*bad_response=*/true);

  EXPECT_EQ(WaitForScreenExitResult(), RecommendAppsScreen::Result::kSkipped);

  const base::Value::List& fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetList(
          arc::prefs::kArcFastAppReinstallPackages);
  EXPECT_EQ(base::Value::List(), fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, ParseError) {
  ShowScreenAndExpectLoadingStep();

  recommend_apps_fetcher_->SimulateParseError();

  EXPECT_EQ(WaitForScreenExitResult(), RecommendAppsScreen::Result::kSkipped);
}

class RecommendAppsScreenManagedTest : public RecommendAppsScreenTest {
 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId("user@example.com", "1111")};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenManagedTest, SkipDueToManagedUser) {
  // Force the sync screen to be shown so that OOBE isn't destroyed
  // right after login due to all screens being skipped.
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;

  // Mark user as managed.
  user_policy_mixin_.RequestPolicyUpdate();

  login_manager_.LoginWithDefaultContext(test_user_);
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  // Skip screens to the tested one.
  LoginDisplayHost::default_host()->StartWizard(
      RecommendAppsScreenView::kScreenId);
  EXPECT_EQ(WaitForScreenExitResult(),
            RecommendAppsScreen::Result::kNotApplicable);
}

}  // namespace
}  // namespace ash
