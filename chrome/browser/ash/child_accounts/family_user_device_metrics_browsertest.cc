// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/family_user_device_metrics.h"

#include <memory>
#include <optional>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {
const AccountId kDefaultOwnerAccountId =
    AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId);
const AccountId kManagedUserAccountId =
    AccountId::FromUserEmail("example@example.com");
const AccountId kActiveDirectoryUserAccountId =
    AccountId::AdFromUserEmailObjGuid("active@gmail.com", "obj-guid");
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

  raw_ptr<FakeChromeUserManager, DanglingUntriaged> user_manager_ = nullptr;

  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(), GetLogInType(),
      /*include_initial_user=*/IsUserExisting()};

  // MixinBasedInProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    if (IsUserExisting()) {
      // Append another user of the same type.
      if (IsUserChild()) {
        logged_in_user_mixin_.GetLoginManagerMixin()->AppendChildUsers(1);
      } else {
        logged_in_user_mixin_.GetLoginManagerMixin()->AppendRegularUsers(1);
      }
    }
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    // Child users require user policy. Set up an empty one so the user can get
    // through login.
    user_policy_mixin_.RequestPolicyUpdate();
    user_manager_ =
        static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
  }

 private:
  LoggedInUserMixin::LogInType GetLogInType() const {
    return std::get<0>(GetParam());
  }

  UserPolicyMixin user_policy_mixin_{&mixin_host_, kDefaultOwnerAccountId};
};

// TODO(crbug.com/40892366): Test is flaky. Too many histogram entries are
// sometimes generated.
#define MAYBE_IsDeviceOwner DISABLED_IsDeviceOwner
IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, MAYBE_IsDeviceOwner) {
  base::HistogramTester histogram_tester;

  // Set the device owner to the logged in user.
  user_manager_->SetOwnerId(logged_in_user_mixin_.GetAccountId());
  logged_in_user_mixin_.LogInUser(
      {ash::LoggedInUserMixin::LoginDetails::kNoBrowserLaunch});

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetDeviceOwnerHistogramNameForTest(),
      /*sample=*/true, /*expected_count=*/1);
}

// TODO(crbug.com/40892366): Test is flaky. Too many histogram entries are
// sometimes generated.
#define MAYBE_IsNotDeviceOwner DISABLED_IsNotDeviceOwner
IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, MAYBE_IsNotDeviceOwner) {
  base::HistogramTester histogram_tester;

  // Set the device owner to an arbitrary account that's not logged in.
  user_manager_->SetOwnerId(kDefaultOwnerAccountId);
  logged_in_user_mixin_.LogInUser(
      {ash::LoggedInUserMixin::LoginDetails::kNoBrowserLaunch});

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetDeviceOwnerHistogramNameForTest(),
      /*sample=*/false, /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, SingleUserAdded) {
  base::HistogramTester histogram_tester;

  logged_in_user_mixin_.LogInUser(
      {ash::LoggedInUserMixin::LoginDetails::kNoBrowserLaunch});

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
  if (!IsUserExisting()) {
    GTEST_SKIP() << "This test makes sense only for existing user";
  }
  base::HistogramTester histogram_tester;

  logged_in_user_mixin_.LogInUser(
      {ash::LoggedInUserMixin::LoginDetails::kNoBrowserLaunch});

  // Current user + extra user from setup.
  const int gaia_users_count = 2;
  const int family_link_users_count = IsUserChild() ? gaia_users_count : 0;

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetFamilyLinkUsersCountHistogramNameForTest(),
      /*sample=*/family_link_users_count,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetGaiaUsersCountHistogramNameForTest(),
      /*sample=*/gaia_users_count,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, LoginAsNewChildUser) {
  if (IsUserExisting() && !IsUserChild()) {
    GTEST_SKIP() << "As this test runs LoginAsNewChildUser"
                    " it is expected that if user exists, it is a child user";
  }
  base::HistogramTester histogram_tester;

  logged_in_user_mixin_.GetLoginManagerMixin()->SkipPostLoginScreens();
  logged_in_user_mixin_.GetLoginManagerMixin()->LoginAsNewChildUser();
  logged_in_user_mixin_.GetLoginManagerMixin()->WaitForActiveSession();

  // If existing Family Link user on login screen, then two Family Link users.
  const int family_link_users_count = IsUserExisting() && IsUserChild() ? 2 : 1;
  // If no existing users on login screen, then this user is the first and only.
  const int gaia_users_count = IsUserExisting() ? 2 : 1;
  // If user existed before, then no users were added.
  const int family_link_users_added = IsUserExisting() ? 0 : 1;

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetNewUserAddedHistogramNameForTest(),
      FamilyUserDeviceMetrics::NewUserAdded::kFamilyLinkUserAdded,
      /*expected_count=*/family_link_users_added);
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetFamilyLinkUsersCountHistogramNameForTest(),
      /*sample=*/family_link_users_count,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics ::GetGaiaUsersCountHistogramNameForTest(),
      /*sample=*/gaia_users_count,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, LoginAsNewRegularUser) {
  base::HistogramTester histogram_tester;

  logged_in_user_mixin_.GetLoginManagerMixin()->SkipPostLoginScreens();
  logged_in_user_mixin_.GetLoginManagerMixin()->LoginAsNewRegularUser();
  logged_in_user_mixin_.GetLoginManagerMixin()->WaitForActiveSession();

  // If existing Family Link user on login screen, then one Family Link user.
  const int family_link_users_count = IsUserExisting() && IsUserChild() ? 1 : 0;
  // If no existing users on login screen, then this user is the first and only.
  const int gaia_users_count = IsUserExisting() ? 2 : 1;
  // If user existed before, then no users were added.
  const int regular_users_added = IsUserExisting() ? 0 : 1;

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetNewUserAddedHistogramNameForTest(),
      FamilyUserDeviceMetrics::NewUserAdded::kRegularUserAdded,
      /*expected_count=*/regular_users_added);
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetFamilyLinkUsersCountHistogramNameForTest(),
      /*sample=*/family_link_users_count,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetGaiaUsersCountHistogramNameForTest(),
      /*sample=*/gaia_users_count,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsTest, GuestUser) {
  base::HistogramTester histogram_tester;

  user_manager_->AddGuestUser();

  logged_in_user_mixin_.GetLoginManagerMixin()->SkipPostLoginScreens();
  logged_in_user_mixin_.GetLoginManagerMixin()->LoginAsNewRegularUser();
  logged_in_user_mixin_.GetLoginManagerMixin()->WaitForActiveSession();

  size_t total_user_count = IsUserExisting() ? 3 : 2;
  EXPECT_EQ(total_user_count, user_manager_->GetUsers().size());

  // If no existing users on login screen, then this user is the first and only.
  const int gaia_users_count = IsUserExisting() ? 2 : 1;

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetGaiaUsersCountHistogramNameForTest(),
      /*sample=*/gaia_users_count,
      /*expected_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FamilyUserDeviceMetricsTest,
    testing::Combine(testing::Values(LoggedInUserMixin::LogInType::kChild,
                                     LoggedInUserMixin::LogInType::kConsumer),
                     /*IsUserExisting=*/testing::Bool()));

class FamilyUserDeviceMetricsManagedDeviceTest
    : public FamilyUserDeviceMetricsTest {
 protected:
  void LoginAsNewRegularUser() {
    logged_in_user_mixin_.GetLoginManagerMixin()->SkipPostLoginScreens();
    logged_in_user_mixin_.GetLoginManagerMixin()->LoginAsNewRegularUser();
    logged_in_user_mixin_.GetLoginManagerMixin()->WaitForActiveSession();
  }

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsManagedDeviceTest, KioskAppUser) {
  base::HistogramTester histogram_tester;

  user_manager_->AddKioskAppUser(kManagedUserAccountId);
  LoginAsNewRegularUser();

  size_t total_user_count = IsUserExisting() ? 3 : 2;
  EXPECT_EQ(total_user_count, user_manager_->GetUsers().size());

  // If no existing users on login screen, then this user is the first and only.
  const int gaia_users_count = IsUserExisting() ? 2 : 1;

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetGaiaUsersCountHistogramNameForTest(),
      /*sample=*/gaia_users_count,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsManagedDeviceTest,
                       WebKioskAppUser) {
  base::HistogramTester histogram_tester;

  user_manager_->AddWebKioskAppUser(kManagedUserAccountId);
  LoginAsNewRegularUser();

  size_t total_user_count = IsUserExisting() ? 3 : 2;
  EXPECT_EQ(total_user_count, user_manager_->GetUsers().size());

  // If no existing users on login screen, then this user is the first and only.
  const int gaia_users_count = IsUserExisting() ? 2 : 1;

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetGaiaUsersCountHistogramNameForTest(),
      /*sample=*/gaia_users_count,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsManagedDeviceTest,
                       PublicAccountUser) {
  base::HistogramTester histogram_tester;

  user_manager_->AddPublicAccountUser(kManagedUserAccountId);
  LoginAsNewRegularUser();

  size_t total_user_count = IsUserExisting() ? 3 : 2;
  EXPECT_EQ(total_user_count, user_manager_->GetUsers().size());

  // If no existing users on login screen, then this user is the first and only.
  const int gaia_users_count = IsUserExisting() ? 2 : 1;

  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetGaiaUsersCountHistogramNameForTest(),
      /*sample=*/gaia_users_count,
      /*expected_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FamilyUserDeviceMetricsManagedDeviceTest,
    testing::Combine(testing::Values(LoggedInUserMixin::LogInType::kChild,
                                     LoggedInUserMixin::LogInType::kConsumer),
                     /*IsUserExisting=*/testing::Bool()));

class FamilyUserDeviceMetricsEphemeralUserTest
    : public FamilyUserDeviceMetricsManagedDeviceTest {
 protected:
  // MixinBaseInProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    device_policy_update->policy_payload()
        ->mutable_ephemeral_users_enabled()
        ->set_ephemeral_users_enabled(true);

    device_policy_update.reset();
  }
};

// Tests that regular ephemeral users report 0 for FamilyUser.GaiaUsersCount.
IN_PROC_BROWSER_TEST_P(FamilyUserDeviceMetricsEphemeralUserTest,
                       EphemeralUser) {
  base::HistogramTester histogram_tester;
  LoginAsNewRegularUser();
  histogram_tester.ExpectUniqueSample(
      FamilyUserDeviceMetrics::GetGaiaUsersCountHistogramNameForTest(),
      /*sample=*/0,
      /*expected_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FamilyUserDeviceMetricsEphemeralUserTest,
    testing::Combine(testing::Values(LoggedInUserMixin::LogInType::kChild,
                                     LoggedInUserMixin::LogInType::kConsumer),
                     /*IsUserExisting=*/testing::Values(false)));

}  // namespace ash
