// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/account_manager/account_manager_policy_controller.h"
#include "chrome/browser/chromeos/account_manager/account_manager_policy_controller_factory.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {
constexpr char kFakePrimaryUsername[] = "test-primary@example.com";
constexpr char kFakeSecondaryUsername[] = "test-secondary@example.com";
constexpr char kFakeSecondaryGaiaId[] = "fake-secondary-gaia-id";
}  // namespace

class AccountManagerPolicyControllerTest : public InProcessBrowserTest {
 public:
  AccountManagerPolicyControllerTest() = default;
  ~AccountManagerPolicyControllerTest() override = default;

  void SetUpOnMainThread() override {
    // Prep private fields.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestProfile"));
    profile_builder.SetProfileName(kFakePrimaryUsername);
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(profile_builder);
    AccountManagerFactory* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    account_manager_ = factory->GetAccountManager(profile()->GetPath().value());
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    // Prep the Primary account.
    auto* identity_test_env =
        identity_test_environment_adaptor_->identity_test_env();
    const AccountInfo primary_account_info =
        identity_test_env->MakePrimaryAccountAvailable(kFakePrimaryUsername);
    auto user_manager = std::make_unique<chromeos::FakeChromeUserManager>();
    primary_account_id_ = AccountId::FromUserEmailGaiaId(
        primary_account_info.email, primary_account_info.gaia);
    const user_manager::User* user = user_manager->AddUser(primary_account_id_);
    user_manager->LoginUser(primary_account_id_);
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile());
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    // Add accounts in Account Manager.
    account_manager_->UpsertAccount(
        AccountManager::AccountKey{
            primary_account_info.gaia,
            account_manager::AccountType::ACCOUNT_TYPE_GAIA},
        primary_account_info.email, AccountManager::kInvalidToken);
    account_manager_->UpsertAccount(
        AccountManager::AccountKey{
            kFakeSecondaryGaiaId,
            account_manager::AccountType::ACCOUNT_TYPE_GAIA},
        kFakeSecondaryUsername, AccountManager::kInvalidToken);

    AccountManagerPolicyControllerFactory::GetForBrowserContext(profile());
  }

  void TearDownOnMainThread() override {
    ProfileHelper::Get()->RemoveUserFromListForTesting(primary_account_id_);
    identity_test_environment_adaptor_.reset();
    profile_.reset();
    base::RunLoop().RunUntilIdle();
    scoped_user_manager_.reset();
  }

  std::vector<AccountManager::Account> GetAccountManagerAccounts() {
    DCHECK(account_manager_);

    std::vector<AccountManager::Account> accounts;
    base::RunLoop run_loop;
    account_manager_->GetAccounts(base::BindLambdaForTesting(
        [&accounts, &run_loop](
            const std::vector<AccountManager::Account>& stored_accounts) {
          accounts = stored_accounts;
          run_loop.Quit();
        }));
    run_loop.Run();

    return accounts;
  }

  Profile* profile() { return profile_.get(); }

  signin::IdentityManager* identity_manager() {
    return identity_test_environment_adaptor_->identity_test_env()
        ->identity_manager();
  }

 private:
  base::ScopedTempDir temp_dir_;
  // Non-owning pointer.
  AccountManager* account_manager_ = nullptr;
  std::unique_ptr<Profile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  AccountId primary_account_id_;

  DISALLOW_COPY_AND_ASSIGN(AccountManagerPolicyControllerTest);
};

IN_PROC_BROWSER_TEST_F(AccountManagerPolicyControllerTest,
                       ExistingSecondaryAccountsAreNotRemovedIfPolicyIsNotSet) {
  std::vector<AccountManager::Account> accounts = GetAccountManagerAccounts();
  // We should have at least 1 Secondary Account.
  const std::vector<AccountManager::Account>::size_type initial_num_accounts =
      accounts.size();
  ASSERT_GT(initial_num_accounts, 1UL);

  // Use default policy value for |kSecondaryGoogleAccountSigninAllowed|
  // (|true|).
  profile()->GetPrefs()->SetBoolean(
      chromeos::prefs::kSecondaryGoogleAccountSigninAllowed, true);

  // All accounts must be intact.
  accounts = GetAccountManagerAccounts();
  EXPECT_EQ(initial_num_accounts, accounts.size());
}

IN_PROC_BROWSER_TEST_F(
    AccountManagerPolicyControllerTest,
    ExistingSecondaryAccountsAreRemovedAfterPolicyApplication) {
  std::vector<AccountManager::Account> accounts = GetAccountManagerAccounts();
  // We should have at least 1 Secondary Account.
  ASSERT_GT(accounts.size(), 1UL);

  // Disallow secondary account sign-ins.
  profile()->GetPrefs()->SetBoolean(
      chromeos::prefs::kSecondaryGoogleAccountSigninAllowed, false);

  // Secondary Accounts must be removed.
  accounts = GetAccountManagerAccounts();
  ASSERT_EQ(accounts.size(), 1UL);
  EXPECT_EQ(ProfileHelper::Get()
                ->GetUserByProfile(profile())
                ->GetAccountId()
                .GetGaiaId(),
            identity_manager()->GetPrimaryAccountInfo().gaia);
  EXPECT_EQ(ProfileHelper::Get()
                ->GetUserByProfile(profile())
                ->GetAccountId()
                .GetGaiaId(),
            accounts[0].key.id);
}

}  // namespace chromeos
