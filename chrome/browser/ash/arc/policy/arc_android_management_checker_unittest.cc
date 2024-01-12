// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/arc_android_management_checker.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/policy/arc/fake_android_management_client.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

using CheckResult = ArcAndroidManagementChecker::CheckResult;

class ArcAndroidManagementCheckerTest : public testing::Test {
 public:
  ArcAndroidManagementCheckerTest() = default;
  ~ArcAndroidManagementCheckerTest() override = default;

 protected:
  enum Options : int {
    kRetryOnError = 1 << 0,
    kMakeAccountAvailable = 1 << 1,
  };

  std::unique_ptr<ArcAndroidManagementChecker> CreateChecker(
      const std::string& profile_email,
      int options) {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(profile_email);
    profile_ = profile_builder.Build();

    auto fake_android_management_client =
        std::make_unique<policy::FakeAndroidManagementClient>();
    fake_android_management_client_ = fake_android_management_client.get();

    if (options & kMakeAccountAvailable)
      identity_test_environment_.MakeAccountAvailable(profile_email);

    signin::IdentityManager* identity_manager =
        identity_test_environment_.identity_manager();
    account_id_ = identity_manager->PickAccountIdForAccount(
        signin::GetTestGaiaIdForEmail(profile_email), profile_email);

    return std::make_unique<ArcAndroidManagementChecker>(
        profile_.get(), identity_manager, account_id_, options & kRetryOnError,
        std::move(fake_android_management_client));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_environment_;

  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<policy::FakeAndroidManagementClient, DanglingUntriaged>
      fake_android_management_client_ = nullptr;
  CoreAccountId account_id_;
};

TEST_F(ArcAndroidManagementCheckerTest, Allowed) {
  auto checker = CreateChecker("user@example.com", kMakeAccountAvailable);

  fake_android_management_client_->SetResult(
      policy::AndroidManagementClient::Result::UNMANAGED);

  base::test::TestFuture<CheckResult> future;
  checker->StartCheck(future.GetCallback());
  EXPECT_EQ(future.Get(), CheckResult::ALLOWED);
  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            1);
}

TEST_F(ArcAndroidManagementCheckerTest, Disallowed) {
  auto checker = CreateChecker("user@example.com", kMakeAccountAvailable);

  fake_android_management_client_->SetResult(
      policy::AndroidManagementClient::Result::MANAGED);

  base::test::TestFuture<CheckResult> future;
  checker->StartCheck(future.GetCallback());
  EXPECT_EQ(future.Get(), CheckResult::DISALLOWED);
  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            1);
}

TEST_F(ArcAndroidManagementCheckerTest, Error) {
  auto checker = CreateChecker("user@example.com", kMakeAccountAvailable);

  fake_android_management_client_->SetResult(
      policy::AndroidManagementClient::Result::ERROR);

  base::test::TestFuture<CheckResult> future;
  checker->StartCheck(future.GetCallback());
  EXPECT_EQ(future.Get(), CheckResult::ERROR);
  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            1);
}

TEST_F(ArcAndroidManagementCheckerTest, NoCheckForManagedUser) {
  auto checker = CreateChecker("user@example.com", kMakeAccountAvailable);
  profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);

  base::test::TestFuture<CheckResult> future;
  checker->StartCheck(future.GetCallback());
  EXPECT_EQ(future.Get(), CheckResult::ALLOWED);
  // No check is performed for a managed user.
  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            0);
}

TEST_F(ArcAndroidManagementCheckerTest, NoCheckForKnownNonEnterpriseDomain) {
  // gmail.com is a known non-enterprise domain.
  auto checker = CreateChecker("user@gmail.com", kMakeAccountAvailable);

  base::test::TestFuture<CheckResult> future;
  checker->StartCheck(future.GetCallback());
  EXPECT_EQ(future.Get(), CheckResult::ALLOWED);
  // No check is performed for a known non-enterprise domain.
  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            0);
}

TEST_F(ArcAndroidManagementCheckerTest, RetryOnError) {
  auto checker =
      CreateChecker("user@example.com", kRetryOnError | kMakeAccountAvailable);

  // Call StartCheck(). This results in getting an error and scheduling a retry.
  fake_android_management_client_->SetResult(
      policy::AndroidManagementClient::Result::ERROR);
  base::test::TestFuture<CheckResult> future;
  checker->StartCheck(future.GetCallback());
  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            1);

  // Change the fake result to UNMANAGED for the retry attempt.
  fake_android_management_client_->SetResult(
      policy::AndroidManagementClient::Result::UNMANAGED);
  EXPECT_EQ(future.Get(), CheckResult::ALLOWED);
  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            2);
}

TEST_F(ArcAndroidManagementCheckerTest, NoRefreshTokenIsAvailable) {
  // No refresh token is available because the account is not available.
  auto checker = CreateChecker("user@example.com", 0);

  fake_android_management_client_->SetResult(
      policy::AndroidManagementClient::Result::UNMANAGED);

  base::test::TestFuture<CheckResult> future;
  checker->StartCheck(future.GetCallback());
  EXPECT_EQ(future.Get(), CheckResult::ERROR);
  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            0);
}

TEST_F(ArcAndroidManagementCheckerTest, OnRefreshTokenUpdatedForAccount) {
  auto checker = CreateChecker("user@example.com", kMakeAccountAvailable);

  // Make the refresh token unavailable. The checker will wait for the refresh
  // token by observing the identity manager.
  identity_test_environment_.RemoveRefreshTokenForAccount(account_id_);

  fake_android_management_client_->SetResult(
      policy::AndroidManagementClient::Result::UNMANAGED);

  base::test::TestFuture<CheckResult> future;
  checker->StartCheck(future.GetCallback());
  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            0);

  // Make the refresh token available to let the checker resume.
  identity_test_environment_.SetRefreshTokenForAccount(account_id_);
  EXPECT_EQ(future.Get(), CheckResult::ALLOWED);
  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            1);
}

}  // namespace

}  // namespace arc
