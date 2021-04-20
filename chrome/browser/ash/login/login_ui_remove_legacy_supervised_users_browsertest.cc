// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_base.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

namespace {

const LoginManagerMixin::TestUserInfo kDeprecatedSupervisedUser{
    AccountId::FromUserEmailGaiaId("test@locally-managed.localhost",
                                   "123456780"),
    user_manager::USER_TYPE_SUPERVISED_DEPRECATED};

const LoginManagerMixin::TestUserInfo kFamilyLinkUser{
    AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId),
    user_manager::USER_TYPE_CHILD};

}  // namespace

class LoginUIRemoveLegacySupervisedUsersTest : public LoginManagerTest {
 public:
  LoginUIRemoveLegacySupervisedUsersTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(1);
  }

 protected:
  LoginManagerMixin login_mixin_{&mixin_host_,
                                 {kDeprecatedSupervisedUser, kFamilyLinkUser}};
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class LoginUIRemoveLegacySupervisedUsersEnabledTest
    : public LoginUIRemoveLegacySupervisedUsersTest {
 public:
  LoginUIRemoveLegacySupervisedUsersEnabledTest()
      : LoginUIRemoveLegacySupervisedUsersTest() {
    scoped_feature_list_.InitAndEnableFeature(
        user_manager::UserManagerBase::kRemoveLegacySupervisedUsersOnStartup);
  }
};

// Verifies that the login screen deletes deprecated supervised users and
// records metrics.
IN_PROC_BROWSER_TEST_F(LoginUIRemoveLegacySupervisedUsersEnabledTest,
                       SupervisedUserHiddenAndDeleted) {
  // Only the two Gaia users (regular and Family Link) should be displayed on
  // the login screen.
  EXPECT_EQ(2, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_EQ(2u, user_manager::UserManager::Get()->GetUsers().size());
  for (const user_manager::User* user :
       user_manager::UserManager::Get()->GetUsers()) {
    EXPECT_TRUE(user->HasGaiaAccount());
    EXPECT_FALSE(user->IsChildOrDeprecatedSupervised() && !user->IsChild());
  }

  histogram_tester_.ExpectBucketCount(
      user_manager::UserManagerBase::kLegacySupervisedUsersHistogramName,
      /*sample=*/
      user_manager::UserManagerBase::LegacySupervisedUserStatus::kLSUHidden,
      /*expected_count=*/0);
  // The login screen deleted one legacy supervised user.
  histogram_tester_.ExpectBucketCount(
      user_manager::UserManagerBase::kLegacySupervisedUsersHistogramName,
      /*sample=*/
      user_manager::UserManagerBase::LegacySupervisedUserStatus::kLSUDeleted,
      /*expected_count=*/1);
  // The login screen displayed one regular user and one Family Link user.
  histogram_tester_.ExpectBucketCount(
      user_manager::UserManagerBase::kLegacySupervisedUsersHistogramName,
      /*sample=*/
      user_manager::UserManagerBase::LegacySupervisedUserStatus::
          kGaiaUserDisplayed,
      /*expected_count=*/2);
  histogram_tester_.ExpectTotalCount(
      user_manager::UserManagerBase::kLegacySupervisedUsersHistogramName, 3);
}

class LoginUIRemoveLegacySupervisedUsersDisabledTest
    : public LoginUIRemoveLegacySupervisedUsersTest {
 public:
  LoginUIRemoveLegacySupervisedUsersDisabledTest()
      : LoginUIRemoveLegacySupervisedUsersTest() {
    scoped_feature_list_.InitAndDisableFeature(
        user_manager::UserManagerBase::kRemoveLegacySupervisedUsersOnStartup);
  }
};

// Tests no users removed.
IN_PROC_BROWSER_TEST_F(LoginUIRemoveLegacySupervisedUsersDisabledTest,
                       NoLegacySupervisedUsersRemoved) {
  // Only the two Gaia users (regular and Family Link) should be displayed on
  // the login screen. The legacy supervised user is still hidden.
  EXPECT_EQ(2, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_EQ(2u, user_manager::UserManager::Get()->GetUsers().size());
  for (const user_manager::User* user :
       user_manager::UserManager::Get()->GetUsers()) {
    EXPECT_TRUE(user->HasGaiaAccount());
    EXPECT_FALSE(user->IsChildOrDeprecatedSupervised() && !user->IsChild());
  }

  histogram_tester_.ExpectBucketCount(
      user_manager::UserManagerBase::kLegacySupervisedUsersHistogramName,
      /*sample=*/
      user_manager::UserManagerBase::LegacySupervisedUserStatus::kLSUHidden,
      /*expected_count=*/1);
  // No users deleted.
  histogram_tester_.ExpectBucketCount(
      user_manager::UserManagerBase::kLegacySupervisedUsersHistogramName,
      /*sample=*/
      user_manager::UserManagerBase::LegacySupervisedUserStatus::kLSUDeleted,
      /*expected_count=*/0);
  // The login screen displayed one regular user and one Family Link user.
  histogram_tester_.ExpectBucketCount(
      user_manager::UserManagerBase::kLegacySupervisedUsersHistogramName,
      /*sample=*/
      user_manager::UserManagerBase::LegacySupervisedUserStatus::
          kGaiaUserDisplayed,
      /*expected_count=*/2);
  histogram_tester_.ExpectTotalCount(
      user_manager::UserManagerBase::kLegacySupervisedUsersHistogramName, 3);
}

}  // namespace chromeos
