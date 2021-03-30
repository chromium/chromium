// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

namespace {

constexpr char kHistogramName[] =
    "ChromeOS.LegacySupervisedUsers.HiddenFromLoginScreen";

const LoginManagerMixin::TestUserInfo kDeprecatedSupervisedUser{
    AccountId::FromUserEmailGaiaId("test@locally-managed.localhost",
                                   "123456780"),
    user_manager::USER_TYPE_SUPERVISED_DEPRECATED};

const LoginManagerMixin::TestUserInfo kFamilyLinkUser{
    AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId),
    user_manager::USER_TYPE_CHILD};

}  // namespace

class LoginUIHideSupervisedUsersTest : public LoginManagerTest {
 public:
  LoginUIHideSupervisedUsersTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
  }

 protected:
  LoginManagerMixin login_mixin_{&mixin_host_,
                                 {kDeprecatedSupervisedUser, kFamilyLinkUser}};
  base::HistogramTester histogram_tester_;
};

// Verifies that the login screen hides deprecated supervised users and records
// metrics.
IN_PROC_BROWSER_TEST_F(LoginUIHideSupervisedUsersTest, SupervisedUserHidden) {
  // Only the Gaia users should be displayed on the login screen.
  EXPECT_EQ(3, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_EQ(3u, user_manager::UserManager::Get()->GetUsers().size());
  for (user_manager::User* user :
       user_manager::UserManager::Get()->GetUsers()) {
    EXPECT_TRUE(!user->IsChildOrDeprecatedSupervised() || user->IsChild());
  }

  // The login screen hid one deprecated supervised user.
  histogram_tester_.ExpectBucketCount(kHistogramName, /*sample=*/true,
                                      /*expected_count=*/1);
  // The login screen displayed two regular users and one Family Link user.
  histogram_tester_.ExpectBucketCount(kHistogramName, /*sample=*/false,
                                      /*expected_count=*/3);
  histogram_tester_.ExpectTotalCount(kHistogramName, 4);
}

}  // namespace chromeos
