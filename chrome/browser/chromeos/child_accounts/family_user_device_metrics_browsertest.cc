// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_user_device_metrics.h"

#include <tuple>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace chromeos {

namespace {

const LoginManagerMixin::TestUserInfo kChild{
    AccountId::FromUserEmailGaiaId("child@gmail.com", "123456780"),
    user_manager::USER_TYPE_CHILD};

const LoginManagerMixin::TestUserInfo kRegular{
    AccountId::FromUserEmailGaiaId("regular@gmail.com", "123456789"),
    user_manager::USER_TYPE_REGULAR};

const AccountId kDefaultOwnerAccountId =
    AccountId::FromUserEmailGaiaId("owner@gmail.com", "123456781");

std::vector<LoginManagerMixin::TestUserInfo> GetInitialUsers(bool with_child) {
  if (with_child)
    return {kChild};
  return {kRegular};
}

}  // namespace

// Test params:
//  - is_initial_user_child: whether the user presented in the initial user list
//  is child.
//  - is_initial_user_device_owner: whether the user presented in the initial
//  user list is device owner.
class FamilyUserDeviceMetricsTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    fake_gaia_.SetupFakeGaiaForChildUser(
        kChild.account_id.GetUserEmail(), kChild.account_id.GetGaiaId(),
        FakeGaiaMixin::kFakeRefreshToken, false /*issue_any_scope_token*/);

    // Child users require a user policy, set up an empty one so the user can
    // get through login.
    ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());

    WizardController::SkipPostLoginScreensForTesting();

    AccountId owner_account = EmptyAccountId();
    if (!IsInitialUserDeviceOwner()) {
      owner_account = kDefaultOwnerAccountId;
    } else if (IsInitialUserChild()) {
      owner_account = kChild.account_id;
    } else {
      owner_account = kRegular.account_id;
    }

    FakeChromeUserManager* user_manager =
        static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
    user_manager->SetOwnerId(owner_account);
    scoped_testing_cros_settings_.device_settings()->Set(
        kDeviceOwner, base::Value(owner_account.GetUserEmail()));
  }

 protected:
  void LoginUser(const LoginManagerMixin::TestUserInfo& user_info) {
    UserContext user_context =
        LoginManagerMixin::CreateDefaultUserContext(user_info);
    user_context.SetRefreshToken(FakeGaiaMixin::kFakeRefreshToken);
    login_manager_mixin_.LoginAndWaitForActiveSession(user_context);
  }

  bool IsLoggedInUserNew(bool is_logged_in_user_child) const {
    // The user list always have 1 initial user, either child user or regular
    // user. If the |is_logged_in_user_child| is different from
    // IsInitialUserChild(), it indicates that the user type of the initial
    // user is different from current logged in user, so the current logged in
    // user is new.
    return is_logged_in_user_child != IsInitialUserChild();
  }

  bool IsLoggedInUserDeviceOwner(bool is_logged_in_user_child) const {
    // If IsInitialUserDeviceOwner() is false, no device owner is set at
    // the beginning, otherwise the initial user is the device owner.
    if (!IsInitialUserDeviceOwner())
      return false;

    // Check whether the current logged in user is the initial user.
    return !IsLoggedInUserNew(is_logged_in_user_child);
  }

  int GetUserCountOnDevice(bool is_logged_in_user_child) const {
    // The user list always have 1 initial user, either child user or regular
    // user. If the current logged in user is new, it has 2 users on the device,
    // otherwise 1.
    return IsLoggedInUserNew(is_logged_in_user_child) ? 2 : 1;
  }

  bool IsInitialUserChild() const { return std::get<0>(GetParam()); }
  bool IsInitialUserDeviceOwner() const { return std::get<1>(GetParam()); }

 private:
  EmbeddedTestServerSetupMixin embedded_test_server_setup_{
      &mixin_host_, embedded_test_server()};
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
  LocalPolicyTestServerMixin policy_server_mixin_{&mixin_host_};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, kChild.account_id,
                                     &policy_server_mixin_};

  LoginManagerMixin login_manager_mixin_{&mixin_host_,
                                         GetInitialUsers(IsInitialUserChild())};
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, LoginAsChildUser) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  LoginUser(kChild);

  // FamilyUserDeviceMetrics::OnNewDay() is triggered in the
  // FamilyUserMetricsService constructor, so that metrics
  // which are reported would have records.
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetFamilyLinkUsersCountHistogramNameForTest(),
      /*sample=*/1,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetTotalUsersCountHistogramNameForTest(),
      /*sample=*/GetUserCountOnDevice(/*is_logged_in_user_child=*/true),
      /*expected_count=*/1);

  if (IsLoggedInUserNew(/*is_logged_in_user_child=*/true)) {
    histogram_tester.ExpectUniqueSample(
        FamilyUserDeviceMetrics::GetNewUserAddedHistogramNameForTest(),
        /*sample=*/FamilyUserDeviceMetrics::NewUserAdded::kFamilyLinkUserAdded,
        /*expected_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        FamilyUserDeviceMetrics::GetNewUserAddedHistogramNameForTest(), 0);
  }

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetDeviceOwnerHistogramNameForTest(),
      /*sample=*/
      IsLoggedInUserDeviceOwner(/*is_logged_in_user_child=*/true) ? 1 : 0,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, LoginAsRegularUser) {
  base::HistogramTester histogram_tester;

  LoginUser(kRegular);

  // FamilyUserDeviceMetrics::OnNewDay() is triggered in the
  // FamilyUserMetricsService constructor, so that metrics
  // which are reported would have records.
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetFamilyLinkUsersCountHistogramNameForTest(),
      /*sample=*/IsInitialUserChild() ? 1 : 0,
      /*expected_count=*/1);

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetTotalUsersCountHistogramNameForTest(),
      /*sample=*/GetUserCountOnDevice(/*is_logged_in_user_child=*/false),
      /*expected_count=*/1);

  if (IsLoggedInUserNew(/*is_logged_in_user_child=*/false)) {
    histogram_tester.ExpectUniqueSample(
        FamilyUserDeviceMetrics::GetNewUserAddedHistogramNameForTest(),
        /*sample=*/FamilyUserDeviceMetrics::NewUserAdded::kRegularUserAdded,
        /*expected_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        FamilyUserDeviceMetrics::GetNewUserAddedHistogramNameForTest(), 0);
  }

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetDeviceOwnerHistogramNameForTest(),
      /*sample=*/
      IsLoggedInUserDeviceOwner(/*is_logged_in_user_child=*/false) ? 1 : 0,
      /*expected_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FamilyUserDeviceMetricsTest,
    testing::Combine(/*is_initial_user_child=*/testing::Bool(),
                     /*is_initial_user_device_owner=*/testing::Bool()));

}  // namespace chromeos
