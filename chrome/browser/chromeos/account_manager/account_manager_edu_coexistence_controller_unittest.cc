// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/account_manager/account_manager_edu_coexistence_controller.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/values.h"
#include "chrome/browser/chromeos/child_accounts/edu_coexistence_tos_store_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/ui/webui/chromeos/edu_coexistence_login_handler_chromeos.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

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
  ::account_manager::Account account;
  account.raw_email = email;
  account.key.id = gaia_id;
  account.key.account_type = account_manager::AccountType::kGaia;
  return account;
}

void AddAccount(AccountManager* account_manager,
                const std::string& email,
                const std::string& gaia_id) {
  ::account_manager::AccountKey account_key;
  account_key.id = gaia_id;
  account_key.account_type = account_manager::AccountType::kGaia;

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

  AccountManager* account_manager() { return account_manager_.get(); }

 private:
  // To support context of browser threads.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<AccountManager> account_manager_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingProfile testing_profile_;
};

void AccountManagerEducoexistenceControllerTest::SetUp() {
  testing_profile_.SetSupervisedUserId(supervised_users::kChildAccountSUID);
  account_manager_ = std::make_unique<AccountManager>();
  account_manager_->SetPrefService(profile()->GetPrefs());

  base::RunLoop run_loop;
  account_manager_->InitializeInEphemeralMode(
      test_url_loader_factory_.GetSafeWeakWrapper(), run_loop.QuitClosure());
  run_loop.Run();

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
  profile()->GetPrefs()->SetString(chromeos::prefs::kEduCoexistenceToSVersion,
                                   new_version);
}

bool AccountManagerEducoexistenceControllerTest::HasInvalidGaiaToken(
    const ::account_manager::Account& account) {
  base::RunLoop run_loop;
  bool is_dummy_return = false;
  account_manager()->HasDummyGaiaToken(
      account.key, base::BindOnce(
                       [](const base::RepeatingClosure& run_loop_callback,
                          bool* out, bool is_invalid) {
                         *out = is_invalid;
                         run_loop_callback.Run();
                       },
                       run_loop.QuitClosure(), &is_dummy_return));
  run_loop.Run();

  return is_dummy_return;
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
                                              kDeviceAccount);
  edu_coexistence_invalidation_controller.Init();

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
                                              kDeviceAccount);
  edu_coexistence_invalidation_controller.Init();

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
                                              kDeviceAccount);
  edu_coexistence_invalidation_controller.Init();

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

}  // namespace chromeos
