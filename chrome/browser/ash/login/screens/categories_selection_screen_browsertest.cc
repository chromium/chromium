// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/categories_selection_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
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
#include "chrome/browser/ui/webui/ash/login/categories_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr char kCategoriesSelectionId[] = "categories-selection";

const test::UIPath kLoadingPath = {kCategoriesSelectionId, "progressDialog"};
const test::UIPath kDialogPath = {kCategoriesSelectionId, "categoriesDialog"};

const test::UIPath kNextButton = {kCategoriesSelectionId, "nextButton"};
const test::UIPath kSkipButton = {kCategoriesSelectionId, "skipButton"};

const test::UIPath kBusinessButton = {kCategoriesSelectionId, "categoriesList",
                                      "cr-button-oobe_business"};
const test::UIPath kEntertaimentButton = {
    kCategoriesSelectionId, "categoriesList", "cr-button-oobe_entertainment"};

oobe::proto::OOBEListResponse GenerateCategoriesList() {
  oobe::proto::OOBEListResponse response;
  auto* tag_1 = response.add_tags();
  tag_1->set_description("");
  tag_1->set_label("Other");
  tag_1->set_tag("oobe_other");
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
  auto* tag_4 = response.add_tags();
  tag_4->set_description("Messaging, video chat, social media");
  tag_4->set_label("Communication");
  tag_4->set_tag("oobe_communication");
  tag_4->set_image_url(
      "https://meltingpot.googleusercontent.com/oobe/communication.svg");
  tag_4->set_order(3);
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

class CategoriesSelectionScreenTest : public OobeBaseTest {
 public:
  CategoriesSelectionScreenTest() {
    feature_list_.InitWithFeatures({ash::features::kOobePersonalizedOnboarding},
                                   {});
  }

  void SetUpOnMainThread() override {
    CategoriesSelectionScreen* categories_selection_screen =
        WizardController::default_controller()
            ->GetScreen<CategoriesSelectionScreen>();

    original_callback_ =
        categories_selection_screen->get_exit_callback_for_testing();
    categories_selection_screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
    categories_selection_screen->set_delay_for_overview_step_for_testing(
        base::Milliseconds(1));

    OobeBaseTest::SetUpOnMainThread();
  }

  void PerformLogin() {
    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  }

  void ShowScreenWithAlmanacError() {
    FakeOobeAppsDiscoveryService fake_service =
        FakeOobeAppsDiscoveryService(ProfileManager::GetActiveUserProfile());
    fake_service.SetAppsFetchingResult(AppsFetchingResult::kErrorMissingKey);
    OobeAppsDiscoveryServiceFactory::GetInstance()
        ->SetOobeAppsDiscoveryServiceForTesting(&fake_service);
    WizardController::default_controller()->AdvanceToScreen(
        CategoriesSelectionScreenView::kScreenId);
  }

  void ShowScreenWithAlmanacSuccess() {
    FakeOobeAppsDiscoveryService fake_service =
        FakeOobeAppsDiscoveryService(ProfileManager::GetActiveUserProfile());
    fake_service.SetAppsFetchingResult(AppsFetchingResult::kSuccess);
    fake_service.SetAppsAndCategories(GenerateCategoriesList());
    OobeAppsDiscoveryServiceFactory::GetInstance()
        ->SetOobeAppsDiscoveryServiceForTesting(&fake_service);
    WizardController::default_controller()->AdvanceToScreen(
        CategoriesSelectionScreenView::kScreenId);
  }

  CategoriesSelectionScreen::Result WaitForScreenExitResult() {
    CategoriesSelectionScreen::Result result = screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};

 private:
  base::test::TestFuture<CategoriesSelectionScreen::Result>
      screen_result_waiter_;
  CategoriesSelectionScreen::ScreenExitCallback original_callback_;
};

IN_PROC_BROWSER_TEST_F(CategoriesSelectionScreenTest, Error) {
  PerformLogin();
  ShowScreenWithAlmanacError();

  OobeScreenWaiter(CategoriesSelectionScreenView::kScreenId).Wait();
  CategoriesSelectionScreen::Result result = WaitForScreenExitResult();

  EXPECT_EQ(result, CategoriesSelectionScreen::Result::kError);
}

IN_PROC_BROWSER_TEST_F(CategoriesSelectionScreenTest, Skip) {
  PerformLogin();
  ShowScreenWithAlmanacSuccess();

  OobeScreenWaiter(CategoriesSelectionScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadingPath)->Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kDialogPath)->Wait();

  test::OobeJS().ExpectDisabledPath(kNextButton);
  test::OobeJS().TapOnPath(kSkipButton);

  CategoriesSelectionScreen::Result result = WaitForScreenExitResult();

  EXPECT_EQ(result, CategoriesSelectionScreen::Result::kSkip);
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kOobeCategoriesSelected));
}

IN_PROC_BROWSER_TEST_F(CategoriesSelectionScreenTest, Next) {
  PerformLogin();
  ShowScreenWithAlmanacSuccess();

  OobeScreenWaiter(CategoriesSelectionScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadingPath)->Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kDialogPath)->Wait();

  test::OobeJS().TapOnPath(kBusinessButton);
  test::OobeJS().TapOnPath(kEntertaimentButton);

  test::OobeJS().TapOnPath(kNextButton);

  CategoriesSelectionScreen::Result result = WaitForScreenExitResult();

  EXPECT_EQ(result, CategoriesSelectionScreen::Result::kNext);

  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_TRUE(prefs->HasPrefPath(prefs::kOobeCategoriesSelected));
  const auto& selected_categories_ids =
      prefs->GetList(prefs::kOobeCategoriesSelected);

  base::Value::List expected_selected_categories_ids;

  expected_selected_categories_ids.Append("oobe_business");
  expected_selected_categories_ids.Append("oobe_entertainment");

  EXPECT_EQ(selected_categories_ids, expected_selected_categories_ids);
}

class CategoriesSelectionScreenManagedTest
    : public CategoriesSelectionScreenTest {
 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId("user@example.com", "1111")};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(CategoriesSelectionScreenManagedTest,
                       SkipDueToManagedUser) {
  // Force the sync screen to be shown so that OOBE isn't destroyed
  // right after login due to all screens being skipped.
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;

  // Mark user as managed.
  user_policy_mixin_.RequestPolicyUpdate();

  login_manager_mixin_.LoginWithDefaultContext(test_user_);
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      CategoriesSelectionScreenView::kScreenId);

  CategoriesSelectionScreen::Result result = WaitForScreenExitResult();

  EXPECT_EQ(result, CategoriesSelectionScreen::Result::kNotApplicable);
}

}  // namespace ash
