// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/personalized_recommend_apps_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_discovery_service.h"
#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_discovery_service_factory.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/personalized_recommend_apps_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr char kPersonalizedRecommendId[] = "personalized-apps";

const test::UIPath kDialogPath = {kPersonalizedRecommendId,
                                  "personalizedRecommendDialog"};

const test::UIPath kNextButton = {kPersonalizedRecommendId, "nextButton"};
const test::UIPath kSkipButton = {kPersonalizedRecommendId, "skipButton"};

const test::UIPath kApps1Checkbox = {
    kPersonalizedRecommendId, "categoriesAppsList", "cr-checkbox-uuid_1"};
const test::UIPath kApps2Checkbox = {
    kPersonalizedRecommendId, "categoriesAppsList", "cr-checkbox-uuid_2"};

oobe::proto::OOBEListResponse GenerateAppsList() {
  oobe::proto::OOBEListResponse response;
  // adding use cases
  auto* tag_1 = response.add_tags();
  tag_1->set_description("");
  tag_1->set_label("Other");
  tag_1->set_tag("oobe_none");
  tag_1->set_image_url(
      "https://meltingpot.googleusercontent.com/oobe/placeholder.svg");
  tag_1->set_order(0);
  auto* tag_2 = response.add_tags();
  tag_2->set_description("Media, music, video streaming");
  tag_2->set_label("Entertainment");
  tag_2->set_tag("oobe_entertainment");
  tag_2->set_image_url(
      "https://meltingpot.googleusercontent.com/oobe/communication.svg");
  tag_2->set_order(1);
  auto* tag_3 = response.add_tags();
  tag_3->set_description("Small business essentials");
  tag_3->set_label("Small Business");
  tag_3->set_tag("oobe_business");
  tag_3->set_image_url(
      "https://meltingpot.googleusercontent.com/oobe/business.svg");
  tag_3->set_order(2);

  auto* app_1 = response.add_apps();
  app_1->set_app_group_uuid("uuid_1");
  app_1->set_package_id("android:package_1");
  app_1->set_name("app_1");
  app_1->add_tags("oobe_none");
  app_1->set_order(1);

  auto* app_2 = response.add_apps();
  app_2->set_app_group_uuid("uuid_2");
  app_2->set_package_id("android:package_2");
  app_2->set_name("app_2");
  app_2->add_tags("oobe_none");
  app_2->set_order(2);

  auto* app_3 = response.add_apps();
  app_3->set_app_group_uuid("uuid_3");
  app_3->set_package_id("web:package_3");
  app_3->set_name("app_3");
  app_3->add_tags("oobe_none");
  app_3->set_order(3);

  return response;
}

}  // namespace

class FakeOobeAppsDiscoveryService : public OobeAppsDiscoveryService {
 public:
  explicit FakeOobeAppsDiscoveryService(Profile* profile)
      : OobeAppsDiscoveryService(profile) {}

  void GetAppsAndUseCases(ResultCallbackAppsAndUseCases callback) override {
    std::move(callback).Run(apps_list_, use_cases_, result_);
  }

  void SetAppsFetchingResult(AppsFetchingResult result) { result_ = result; }

  void SetAppsAndCategories(oobe::proto::OOBEListResponse response) {
    for (oobe::proto::OOBEListResponse::App app_info : response.apps()) {
      apps_list_.emplace_back(std::move(app_info));
    }
    for (oobe::proto::OOBEListResponse::Tag usecase : response.tags()) {
      use_cases_.emplace_back(std::move(usecase));
    }
  }

 private:
  std::vector<OOBEAppDefinition> apps_list_;
  std::vector<OOBEDeviceUseCase> use_cases_;
  AppsFetchingResult result_;
};

class PersonalizedRecommendAppsScreenTest : public OobeBaseTest {
 public:
  PersonalizedRecommendAppsScreenTest() {
    feature_list_.InitWithFeatures({ash::features::kOobePersonalizedOnboarding},
                                   {});
  }

  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
  }

  void SetUpOnMainThread() override {
    PersonalizedRecommendAppsScreen* presonalized_recommend_screen =
        WizardController::default_controller()
            ->GetScreen<PersonalizedRecommendAppsScreen>();
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    original_callback_ =
        presonalized_recommend_screen->get_exit_callback_for_testing();
    presonalized_recommend_screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
    presonalized_recommend_screen->set_delay_for_overview_step_for_testing(
        base::Milliseconds(1));
    presonalized_recommend_screen->set_delay_for_set_apps_for_testing(
        base::Milliseconds(1));

    OobeBaseTest::SetUpOnMainThread();
  }

  void PerformLogin() {
    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  }

  void ShowRecommendAppsScreenWithAlmanacError() {
    FakeOobeAppsDiscoveryService fake_service =
        FakeOobeAppsDiscoveryService(ProfileManager::GetActiveUserProfile());
    fake_service.SetAppsFetchingResult(AppsFetchingResult::kErrorMissingKey);
    OobeAppsDiscoveryServiceFactory::GetInstance()
        ->SetOobeAppsDiscoveryServiceForTesting(&fake_service);
    WizardController::default_controller()->AdvanceToScreen(
        PersonalizedRecommendAppsScreenView::kScreenId);
  }

  void ShowRecommendAppsScreenWithAlmanacSuccess() {
    FakeOobeAppsDiscoveryService fake_service =
        FakeOobeAppsDiscoveryService(ProfileManager::GetActiveUserProfile());
    fake_service.SetAppsFetchingResult(AppsFetchingResult::kSuccess);
    fake_service.SetAppsAndCategories(GenerateAppsList());
    OobeAppsDiscoveryServiceFactory::GetInstance()
        ->SetOobeAppsDiscoveryServiceForTesting(&fake_service);
    WizardController::default_controller()->AdvanceToScreen(
        PersonalizedRecommendAppsScreenView::kScreenId);
  }

  PersonalizedRecommendAppsScreen::Result WaitForScreenExitResult() {
    PersonalizedRecommendAppsScreen::Result result =
        screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};

 private:
  base::test::TestFuture<PersonalizedRecommendAppsScreen::Result>
      screen_result_waiter_;
  PersonalizedRecommendAppsScreen::ScreenExitCallback original_callback_;
};

IN_PROC_BROWSER_TEST_F(PersonalizedRecommendAppsScreenTest, WithOldUser) {
  PerformLogin();

  // Setting the user to an existing chromeos user.
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kOobeMarketingOptInScreenFinished, true);

  WizardController::default_controller()->AdvanceToScreen(
      PersonalizedRecommendAppsScreenView::kScreenId);

  PersonalizedRecommendAppsScreen::Result result = WaitForScreenExitResult();

  EXPECT_EQ(result, PersonalizedRecommendAppsScreen::Result::kNotApplicable);
}

IN_PROC_BROWSER_TEST_F(PersonalizedRecommendAppsScreenTest, ServerError) {
  PerformLogin();

  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kOobeMarketingOptInScreenFinished, false);

  ShowRecommendAppsScreenWithAlmanacError();

  OobeScreenWaiter(PersonalizedRecommendAppsScreenView::kScreenId).Wait();
  PersonalizedRecommendAppsScreen::Result result = WaitForScreenExitResult();
  EXPECT_EQ(result, PersonalizedRecommendAppsScreen::Result::kError);
}

IN_PROC_BROWSER_TEST_F(PersonalizedRecommendAppsScreenTest, Skip) {
  PerformLogin();
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kOobeMarketingOptInScreenFinished, false);

  ShowRecommendAppsScreenWithAlmanacSuccess();

  test::OobeJS().CreateVisibilityWaiter(true, kDialogPath)->Wait();

  test::OobeJS().ExpectDisabledPath(kNextButton);
  test::OobeJS().TapOnPath(kSkipButton);

  PersonalizedRecommendAppsScreen::Result result = WaitForScreenExitResult();

  EXPECT_EQ(result, PersonalizedRecommendAppsScreen::Result::kSkip);
}

IN_PROC_BROWSER_TEST_F(PersonalizedRecommendAppsScreenTest, Next) {
  PerformLogin();
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kOobeMarketingOptInScreenFinished, false);

  ShowRecommendAppsScreenWithAlmanacSuccess();

  OobeScreenWaiter(PersonalizedRecommendAppsScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kDialogPath)->Wait();

  test::OobeJS().TapOnPath(kApps1Checkbox);
  test::OobeJS().TapOnPath(kApps2Checkbox);

  test::OobeJS().TapOnPath(kNextButton);

  PersonalizedRecommendAppsScreen::Result result = WaitForScreenExitResult();

  EXPECT_EQ(result, PersonalizedRecommendAppsScreen::Result::kNext);
  // TODO(b/342623828): Add testing the apps install logic
}

class PersonalizedRecommendAppsScreenManagedTest
    : public PersonalizedRecommendAppsScreenTest {
 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId("user@example.com", "1111")};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(PersonalizedRecommendAppsScreenManagedTest,
                       SkipDueToManagedUser) {
  // Force the sync screen to be shown so that OOBE isn't destroyed
  // right after login due to all screens being skipped.
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;

  // Mark user as managed.
  user_policy_mixin_.RequestPolicyUpdate();

  login_manager_mixin_.LoginWithDefaultContext(test_user_);
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  // Skip screens to the tested one.
  // LoginDisplayHost::default_host()->StartWizard(
  //  RecommendAppsScreenView::kScreenId);
  WizardController::default_controller()->AdvanceToScreen(
      PersonalizedRecommendAppsScreenView::kScreenId);

  PersonalizedRecommendAppsScreen::Result result = WaitForScreenExitResult();

  EXPECT_EQ(result, PersonalizedRecommendAppsScreen::Result::kNotApplicable);
}

}  // namespace ash
