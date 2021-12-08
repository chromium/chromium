// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_apps_availability.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_base.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Contains;

namespace ash {

namespace {

constexpr char kPrimaryAccountEmail[] = "primaryaccount@gmail.com";

}  // namespace

class AccountAppsAvailabilityTest : public testing::Test {
 protected:
  AccountAppsAvailabilityTest() = default;
  AccountAppsAvailabilityTest(const AccountAppsAvailabilityTest&) = delete;
  AccountAppsAvailabilityTest& operator=(const AccountAppsAvailabilityTest&) =
      delete;
  ~AccountAppsAvailabilityTest() override = default;

  void SetUp() override {
    identity_test_env()->EnableRemovalOfExtendedAccountInfo();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    AccountAppsAvailability::RegisterPrefs(pref_service_->registry());

    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager_ = new user_manager::FakeUserManager();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));

    primary_account_ = identity_test_env()->MakePrimaryAccountAvailable(
        kPrimaryAccountEmail, signin::ConsentLevel::kSignin);
    LoginUserSession();
  }

  void TearDown() override { pref_service_.reset(); }

  std::unique_ptr<AccountAppsAvailability> CreateAccountAppsAvailability() {
    return std::make_unique<AccountAppsAvailability>(
        GetAccountManagerFacade(identity_test_env()->identity_manager()),
        identity_test_env()->identity_manager(), pref_service_.get());
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  AccountInfo* primary_account_info() { return &primary_account_; }

 private:
  void LoginUserSession() {
    auto account_id = AccountId::FromUserEmailGaiaId(primary_account_.email,
                                                     primary_account_.gaia);
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(
        account_id, account_id.GetUserEmail() + "-hash", false, false);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  AccountInfo primary_account_;
  // Owned by `scoped_user_manager_`.
  user_manager::FakeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(AccountAppsAvailabilityTest, InitializationPrefIsPersistedOnDisk) {
  auto account_apps_availability = CreateAccountAppsAvailability();
  EXPECT_FALSE(account_apps_availability->IsInitialized());
  // Wait for `GetAccounts` call to finish.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(account_apps_availability->IsInitialized());
  account_apps_availability.reset();

  account_apps_availability = CreateAccountAppsAvailability();
  EXPECT_TRUE(account_apps_availability->IsInitialized());
}

}  // namespace ash
