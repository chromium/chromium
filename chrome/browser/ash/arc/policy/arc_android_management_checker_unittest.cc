// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/arc_android_management_checker.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/policy/arc/fake_android_management_client.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

using CheckResult = ArcAndroidManagementChecker::CheckResult;

// TODO(b/247050850): Add tests for the refresh token handling and retry code.
class ArcAndroidManagementCheckerTest : public testing::Test {
 public:
  ArcAndroidManagementCheckerTest() = default;
  ~ArcAndroidManagementCheckerTest() override = default;

 protected:
  std::unique_ptr<ArcAndroidManagementChecker> CreateChecker(
      const std::string& profile_email) {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(profile_email);
    profile_ = profile_builder.Build();

    auto fake_android_management_client =
        std::make_unique<policy::FakeAndroidManagementClient>();
    fake_android_management_client_ = fake_android_management_client.get();

    identity_test_environment_.MakeAccountAvailable(profile_email);

    signin::IdentityManager* identity_manager =
        identity_test_environment_.identity_manager();
    CoreAccountId account_id = identity_manager->PickAccountIdForAccount(
        signin::GetTestGaiaIdForEmail(profile_email), profile_email);

    return std::make_unique<ArcAndroidManagementChecker>(
        profile_.get(), identity_manager, account_id, false,
        std::move(fake_android_management_client));
  }

  content::BrowserTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;

  std::unique_ptr<TestingProfile> profile_;
  policy::FakeAndroidManagementClient* fake_android_management_client_ =
      nullptr;
};

TEST_F(ArcAndroidManagementCheckerTest, Allowed) {
  auto checker = CreateChecker("user@example.com");

  fake_android_management_client_->SetResult(
      policy::AndroidManagementClient::Result::UNMANAGED);

  base::RunLoop run_loop;
  checker->StartCheck(base::BindLambdaForTesting([&](CheckResult result) {
    EXPECT_EQ(result, CheckResult::ALLOWED);
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            1);
}

TEST_F(ArcAndroidManagementCheckerTest, Disallowed) {
  auto checker = CreateChecker("user@example.com");

  fake_android_management_client_->SetResult(
      policy::AndroidManagementClient::Result::MANAGED);

  base::RunLoop run_loop;
  checker->StartCheck(base::BindLambdaForTesting([&](CheckResult result) {
    EXPECT_EQ(result, CheckResult::DISALLOWED);
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            1);
}

TEST_F(ArcAndroidManagementCheckerTest, Error) {
  auto checker = CreateChecker("user@example.com");

  fake_android_management_client_->SetResult(
      policy::AndroidManagementClient::Result::ERROR);

  base::RunLoop run_loop;
  checker->StartCheck(base::BindLambdaForTesting([&](CheckResult result) {
    EXPECT_EQ(result, CheckResult::ERROR);
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            1);
}

TEST_F(ArcAndroidManagementCheckerTest, NoCheckForManagedUser) {
  auto checker = CreateChecker("user@example.com");
  profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);

  base::RunLoop run_loop;
  checker->StartCheck(base::BindLambdaForTesting([&](CheckResult result) {
    EXPECT_EQ(result, CheckResult::ALLOWED);
    run_loop.Quit();
  }));
  run_loop.Run();

  // No check is performed for a managed user.
  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            0);
}

TEST_F(ArcAndroidManagementCheckerTest, NoCheckForKnownNonEnterpriseDomain) {
  // gmail.com is a known non-enterprise domain.
  auto checker = CreateChecker("user@gmail.com");

  base::RunLoop run_loop;
  checker->StartCheck(base::BindLambdaForTesting([&](CheckResult result) {
    EXPECT_EQ(result, CheckResult::ALLOWED);
    run_loop.Quit();
  }));
  run_loop.Run();

  // No check is performed for a known non-enterprise domain.
  EXPECT_EQ(fake_android_management_client_
                ->start_check_android_management_call_count(),
            0);
}

}  // namespace

}  // namespace arc
