// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_user_device_metrics.h"

#include <tuple>

#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/login/test/logged_in_user_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

namespace {
const AccountId kDefaultOwnerAccountId =
    AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId);
}  // namespace

// Test params:
//  - LogInType: regular or child.
//  - IsUserExisting: if false, no existing users on the login screen.
class FamilyUserDeviceMetricsTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<
          std::tuple<LoggedInUserMixin::LogInType, /*IsUserExisting=*/bool>> {
 protected:
  bool IsUserChild() const {
    return GetLogInType() == LoggedInUserMixin::LogInType::kChild;
  }
  bool IsUserExisting() const { return std::get<1>(GetParam()); }

  FakeChromeUserManager* user_manager_ = nullptr;

  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_,
      GetLogInType(),
      embedded_test_server(),
      this,
      /*should_launch_browser=*/false,
      /*account_id=*/base::nullopt,
      /*include_initial_user=*/IsUserExisting()};

 private:
  // MixinBasedInProcessBrowserTest:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    // Child users require user policy. Set up an empty one so the user can get
    // through login.
    user_policy_mixin_.RequestPolicyUpdate();
    user_manager_ =
        static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
  }

  LoggedInUserMixin::LogInType GetLogInType() const {
    return std::get<0>(GetParam());
  }

  UserPolicyMixin user_policy_mixin_{&mixin_host_, kDefaultOwnerAccountId};
};

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, IsDeviceOwner) {
  base::HistogramTester histogram_tester;

  // Set the device owner to the logged in user.
  user_manager_->SetOwnerId(logged_in_user_mixin_.GetAccountId());
  logged_in_user_mixin_.LogInUser();

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetDeviceOwnerHistogramNameForTest(),
      /*sample=*/true, /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, IsNotDeviceOwner) {
  base::HistogramTester histogram_tester;

  // Set the device owner to an arbitrary account that's not logged in.
  user_manager_->SetOwnerId(kDefaultOwnerAccountId);
  logged_in_user_mixin_.LogInUser();

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetDeviceOwnerHistogramNameForTest(),
      /*sample=*/false, /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, SingleUserAdded) {
  base::HistogramTester histogram_tester;

  logged_in_user_mixin_.LogInUser();

  if (IsUserExisting()) {
    // This user has signed into this device before, so they are not new.
    histogram_tester.ExpectTotalCount(
        FamilyUserDeviceMetrics::GetNewUserAddedHistogramNameForTest(), 0);
  } else {
    // This is the first time this user is signing into this device.
    FamilyUserDeviceMetrics::NewUserAdded sample =
        IsUserChild()
            ? FamilyUserDeviceMetrics::NewUserAdded::kFamilyLinkUserAdded
            : FamilyUserDeviceMetrics::NewUserAdded::kRegularUserAdded;
    histogram_tester.ExpectUniqueSample(
        FamilyUserDeviceMetrics::GetNewUserAddedHistogramNameForTest(), sample,
        /*expected_count=*/1);
  }
}

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, SingleUserCount) {
  base::HistogramTester histogram_tester;

  logged_in_user_mixin_.LogInUser();

  const int family_link_users_count = IsUserChild() ? 1 : 0;
  const int total_users_count = 1;

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetFamilyLinkUsersCountHistogramNameForTest(),
      /*sample=*/family_link_users_count,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetTotalUsersCountHistogramNameForTest(),
      /*sample=*/total_users_count,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, LoginAsNewChildUser) {
  base::HistogramTester histogram_tester;

  logged_in_user_mixin_.GetLoginManagerMixin()->LoginAsNewChildUser();
  logged_in_user_mixin_.GetLoginManagerMixin()->WaitForActiveSession();

  // If existing Family Link user on login screen, then two Family Link users.
  const int family_link_users_count = IsUserExisting() && IsUserChild() ? 2 : 1;
  // If no existing users on login screen, then this user is the first and only.
  const int total_users_count = IsUserExisting() ? 2 : 1;

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetNewUserAddedHistogramNameForTest(),
      FamilyUserDeviceMetrics::NewUserAdded::kFamilyLinkUserAdded,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetFamilyLinkUsersCountHistogramNameForTest(),
      /*sample=*/family_link_users_count,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetTotalUsersCountHistogramNameForTest(),
      /*sample=*/total_users_count,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, LoginAsNewRegularUser) {
  base::HistogramTester histogram_tester;

  logged_in_user_mixin_.GetLoginManagerMixin()->LoginAsNewRegularUser();
  logged_in_user_mixin_.GetLoginManagerMixin()->WaitForActiveSession();

  // If existing Family Link user on login screen, then one Family Link user.
  const int family_link_users_count = IsUserExisting() && IsUserChild() ? 1 : 0;
  // If no existing users on login screen, then this user is the first and only.
  const int total_users_count = IsUserExisting() ? 2 : 1;

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetNewUserAddedHistogramNameForTest(),
      FamilyUserDeviceMetrics::NewUserAdded::kRegularUserAdded,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetFamilyLinkUsersCountHistogramNameForTest(),
      /*sample=*/family_link_users_count,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetTotalUsersCountHistogramNameForTest(),
      /*sample=*/total_users_count,
      /*expected_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FamilyUserDeviceMetricsTest,
    testing::Combine(testing::Values(LoggedInUserMixin::LogInType::kChild,
                                     LoggedInUserMixin::LogInType::kRegular),
                     /*IsUserExisting=*/testing::Bool()));

}  // namespace chromeos
