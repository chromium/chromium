// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/user_delegate_impl.h"

#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_signals {

namespace {
constexpr char kUserEmail[] = "someEmail@example.com";
constexpr char kOtherUserEmail[] = "someOtherUser@example.com";
constexpr char kOtherUserGaiaId[] = "some-other-user-gaia";
}  // namespace

class UserDelegateImplTest : public testing::Test {
 protected:
  std::unique_ptr<TestingProfile> CreateProfile(bool is_managed) {
    TestingProfile::Builder builder;
    builder.OverridePolicyConnectorIsManagedForTesting(is_managed);
    return builder.Build();
  }

  content::BrowserTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
};

// Tests that IsManaged returns false when the user is not managed.
TEST_F(UserDelegateImplTest, IsManaged_False) {
  auto test_profile = CreateProfile(/*is_managed=*/false);

  UserDelegateImpl user_delegate(test_profile.get(),
                                 identity_test_env_.identity_manager());
  EXPECT_FALSE(user_delegate.IsManaged());
}

// Tests that IsManaged returns true when the user is managed.
TEST_F(UserDelegateImplTest, IsManaged_True) {
  auto test_profile = CreateProfile(/*is_managed=*/true);

  UserDelegateImpl user_delegate(test_profile.get(),
                                 identity_test_env_.identity_manager());
  EXPECT_TRUE(user_delegate.IsManaged());
}

// Tests that IsSameUser returns false when given a different user.
TEST_F(UserDelegateImplTest, IsSameManagedUser_DifferentUser) {
  auto test_profile = CreateProfile(/*is_managed=*/true);
  auto account = identity_test_env_.MakePrimaryAccountAvailable(
      kUserEmail, signin::ConsentLevel::kSignin);
  auto other_account = identity_test_env_.MakeAccountAvailableWithCookies(
      kOtherUserEmail, kOtherUserGaiaId);

  UserDelegateImpl user_delegate(test_profile.get(),
                                 identity_test_env_.identity_manager());
  EXPECT_FALSE(user_delegate.IsSameUser(kOtherUserGaiaId));
}

// Tests that IsSameUser returns false when there is no primary user.
TEST_F(UserDelegateImplTest, IsSameUser_NoPrimaryUser) {
  auto test_profile = CreateProfile(/*is_managed=*/true);
  auto other_account = identity_test_env_.MakeAccountAvailableWithCookies(
      kOtherUserEmail, kOtherUserGaiaId);

  UserDelegateImpl user_delegate(test_profile.get(),
                                 identity_test_env_.identity_manager());
  EXPECT_FALSE(user_delegate.IsSameUser(kOtherUserGaiaId));
}

// Tests that IsSameUser returns true when given the same user, and the
// user did not give Sync consent.
TEST_F(UserDelegateImplTest, IsSameUser_SameUser_Signin) {
  auto test_profile = CreateProfile(/*is_managed=*/true);
  auto account = identity_test_env_.MakePrimaryAccountAvailable(
      kUserEmail, signin::ConsentLevel::kSignin);

  UserDelegateImpl user_delegate(test_profile.get(),
                                 identity_test_env_.identity_manager());
  EXPECT_TRUE(user_delegate.IsSameUser(account.gaia));
}

// Tests that IsSameUser returns true when given the same user, and the
// user gave Sync consent.
TEST_F(UserDelegateImplTest, IsSameUser_SameUser_Sync) {
  auto test_profile = CreateProfile(/*is_managed=*/true);
  auto account = identity_test_env_.MakePrimaryAccountAvailable(
      kUserEmail, signin::ConsentLevel::kSync);

  UserDelegateImpl user_delegate(test_profile.get(),
                                 identity_test_env_.identity_manager());
  EXPECT_TRUE(user_delegate.IsSameUser(account.gaia));
}

}  // namespace enterprise_signals
