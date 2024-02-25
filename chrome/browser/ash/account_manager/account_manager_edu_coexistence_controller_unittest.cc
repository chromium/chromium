// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_manager_edu_coexistence_controller.h"

#include <memory>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/edu_coexistence_tos_store_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/edu_coexistence/edu_coexistence_login_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kValidToken[] = "valid-token";

constexpr char kPrimaryAccount[] = "primaryaccount@gmail.com";
constexpr char kPrimaryAccountGaiaId[] = "primary-account-id";

constexpr char kSecondaryAccount1[] = "secondaryAccount1@gmail.com";
constexpr char kSecondaryAccount1GaiaId[] = "secondary-account-1";

constexpr char kSecondaryAccount2[] = "secondaryAccount2@gmail.com";
constexpr char kSecondaryAccount2GaiaId[] = "secondary-account-2";

constexpr char kSecondaryAccount3[] = "secondaryAccount3@gmail.com";
constexpr char kSecondaryAccount3GaiaId[] = "secondary-account-3";

constexpr char kSecondaryAccount4[] = "secondaryAccount4@gmail.com";
constexpr char kSecondaryAccount4GaiaId[] = "secondary-account-4";

const AccountId kDeviceAccount =
    AccountId::FromUserEmailGaiaId(kPrimaryAccount, kPrimaryAccountGaiaId);

::account_manager::Account GetAccountFor(const std::string& email,
                                         const std::string& gaia_id) {
  ::account_manager::AccountKey key(gaia_id,
                                    ::account_manager::AccountType::kGaia);
  return {key, email};
}

void AddAccount(account_manager::AccountManager* account_manager,
                const std::string& email,
                const std::string& gaia_id) {
  ::account_manager::AccountKey account_key(
      gaia_id, ::account_manager::AccountType::kGaia);
  account_manager->UpsertAccount(account_key, email, kValidToken);
}

}  // namespace

class AccountManagerEducoexistenceControllerTest : public testing::Test {
 public:
  AccountManagerEducoexistenceControllerTest() = default;
  AccountManagerEducoexistenceControllerTest(
      const AccountManagerEducoexistenceControllerTest&) = delete;
  AccountManagerEducoexistenceControllerTest& operator=(
      const AccountManagerEducoexistenceControllerTest&) = delete;
  ~AccountManagerEducoexistenceControllerTest() override = default;

  void SetUp() override;

  void UpdatEduCoexistenceToSAcceptedVersion(const std::string& email,
                                             const std::string& tosVersion);

  void UpdateEduCoexistenceToSVersion(const std::string& new_version);

  bool HasInvalidGaiaToken(const ::account_manager::Account& account);

 protected:
  Profile* profile() { return &testing_profile_; }

  account_manager::AccountManager* account_manager() {
    return account_manager_;
  }
  account_manager::AccountManagerFacade* account_manager_facade() {
    return account_manager_facade_;
  }

 private:
  // To support context of browser threads.
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<account_manager::AccountManager> account_manager_ = nullptr;
  raw_ptr<account_manager::AccountManagerFacade> account_manager_facade_ =
      nullptr;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingProfile testing_profile_;
};

void AccountManagerEducoexistenceControllerTest::SetUp() {
  testing_profile_.SetIsSupervisedProfile();
  account_manager_ = g_browser_process->platform_part()
                         ->GetAccountManagerFactory()
                         ->GetAccountManager(profile()->GetPath().value());
  account_manager_facade_ =
      ::GetAccountManagerFacade(profile()->GetPath().value());

  AddAccount(account_manager(), kPrimaryAccount, kPrimaryAccountGaiaId);
}

void AccountManagerEducoexistenceControllerTest::
    UpdatEduCoexistenceToSAcceptedVersion(const std::string& gaia_id,
                                          const std::string& tosVersion) {
  edu_coexistence::UpdateAcceptedToSVersionPref(
      profile(), edu_coexistence::UserConsentInfo(gaia_id, tosVersion));
}

void AccountManagerEducoexistenceControllerTest::UpdateEduCoexistenceToSVersion(
    const std::string& new_version) {
  profile()->GetPrefs()->SetString(prefs::kEduCoexistenceToSVersion,
                                   new_version);
}

bool AccountManagerEducoexistenceControllerTest::HasInvalidGaiaToken(
    const ::account_manager::Account& account) {
  base::test::TestFuture<bool> future;
  account_manager()->HasDummyGaiaToken(account.key, future.GetCallback());
  return future.Get();
}

TEST_F(AccountManagerEducoexistenceControllerTest,
       AccountsInPrefWithInvalidTokenShouldBeInvalidated) {
  // Account will be invalidated.
  UpdatEduCoexistenceToSAcceptedVersion(kSecondaryAccount1GaiaId, "0");
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile(),
                                                   kSecondaryAccount1GaiaId),
            "0");

  // Account will not be invalidated fine.
  UpdatEduCoexistenceToSAcceptedVersion(kSecondaryAccount2GaiaId, "5");
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile(),
                                                   kSecondaryAccount2GaiaId),
            "5");

  AddAccount(account_manager(), kSecondaryAccount1, kSecondaryAccount1GaiaId);
  AddAccount(account_manager(), kSecondaryAccount2, kSecondaryAccount2GaiaId);
  EXPECT_FALSE(HasInvalidGaiaToken(
      GetAccountFor(kSecondaryAccount1, kSecondaryAccount1GaiaId)));
  EXPECT_FALSE(HasInvalidGaiaToken(
      GetAccountFor(kSecondaryAccount2, kSecondaryAccount2GaiaId)));

  UpdateEduCoexistenceToSVersion("5");

  EduCoexistenceConsentInvalidationController
      edu_coexistence_invalidation_controller(profile(), account_manager(),
                                              account_manager_facade(),
                                              kDeviceAccount);
  edu_coexistence_invalidation_controller.Init();

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(HasInvalidGaiaToken(
      GetAccountFor(kSecondaryAccount1, kSecondaryAccount1GaiaId)));
  EXPECT_FALSE(HasInvalidGaiaToken(
      GetAccountFor(kSecondaryAccount2, kSecondaryAccount2GaiaId)));
}

TEST_F(AccountManagerEducoexistenceControllerTest,
       AccountsNotInPrefShouldBeAddedToUserPref) {
  UpdatEduCoexistenceToSAcceptedVersion(kSecondaryAccount1GaiaId, "5");
  UpdatEduCoexistenceToSAcceptedVersion(kSecondaryAccount2GaiaId, "6");
  UpdatEduCoexistenceToSAcceptedVersion(kSecondaryAccount3GaiaId, "7");

  AddAccount(account_manager(), kSecondaryAccount1, kSecondaryAccount1GaiaId);
  AddAccount(account_manager(), kSecondaryAccount2, kSecondaryAccount2GaiaId);
  AddAccount(account_manager(), kSecondaryAccount3, kSecondaryAccount3GaiaId);

  // Note: kSecondaryAccount4 is not present in pref.
  AddAccount(account_manager(), kSecondaryAccount4, kSecondaryAccount4GaiaId);

  EduCoexistenceConsentInvalidationController
      edu_coexistence_invalidation_controller(profile(), account_manager(),
                                              account_manager_facade(),
                                              kDeviceAccount);
  edu_coexistence_invalidation_controller.Init();

  base::RunLoop().RunUntilIdle();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile(),
                                                   kSecondaryAccount4GaiaId),
            edu_coexistence::kMinTOSVersionNumber);
}

TEST_F(AccountManagerEducoexistenceControllerTest,
       AccountsNotInAccountManagerShouldBeRemovedFromUserPref) {
  UpdatEduCoexistenceToSAcceptedVersion(kSecondaryAccount1GaiaId, "5");
  UpdatEduCoexistenceToSAcceptedVersion(kSecondaryAccount2GaiaId, "6");
  UpdatEduCoexistenceToSAcceptedVersion(kSecondaryAccount3GaiaId, "7");

  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile(),
                                                   kSecondaryAccount1GaiaId),
            "5");
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile(),
                                                   kSecondaryAccount2GaiaId),
            "6");
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile(),
                                                   kSecondaryAccount3GaiaId),
            "7");

  // kSecondaryAccount1 was unfortunately removed and is not present in account
  // manager.
  AddAccount(account_manager(), kSecondaryAccount2, kSecondaryAccount2GaiaId);
  AddAccount(account_manager(), kSecondaryAccount3, kSecondaryAccount3GaiaId);

  EduCoexistenceConsentInvalidationController
      edu_coexistence_invalidation_controller(profile(), account_manager(),
                                              account_manager_facade(),
                                              kDeviceAccount);
  edu_coexistence_invalidation_controller.Init();

  base::RunLoop().RunUntilIdle();

  // kSecondaryAccount1 is not present
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile(),
                                                   kSecondaryAccount1GaiaId),
            "");

  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile(),
                                                   kSecondaryAccount2GaiaId),
            "6");
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile(),
                                                   kSecondaryAccount3GaiaId),
            "7");
}

}  // namespace ash
