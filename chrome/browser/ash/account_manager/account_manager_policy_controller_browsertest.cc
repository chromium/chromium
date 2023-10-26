// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/ash/account_manager/account_manager_policy_controller.h"
#include "chrome/browser/ash/account_manager/account_manager_policy_controller_factory.h"
#include "chrome/browser/ash/account_manager/child_account_type_changed_user_data.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/account_manager_core/pref_names.h"
#include "components/signin/public/base/consent_level.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kFakePrimaryUsername[] = "test-primary@example.com";
constexpr char kFakeSecondaryUsername[] = "test-secondary@example.com";
constexpr char kFakeSecondaryGaiaId[] = "fake-secondary-gaia-id";

}  // namespace

class AccountManagerPolicyControllerTest : public InProcessBrowserTest {
 public:
  AccountManagerPolicyControllerTest() = default;

  AccountManagerPolicyControllerTest(
      const AccountManagerPolicyControllerTest&) = delete;
  AccountManagerPolicyControllerTest& operator=(
      const AccountManagerPolicyControllerTest&) = delete;

  ~AccountManagerPolicyControllerTest() override = default;

  void SetUpOnMainThread() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    // Prep private fields.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII(
        BrowserContextHelper::GetUserBrowserContextDirName(
            user_manager::FakeUserManager::GetFakeUsernameHash(
                AccountId::FromUserEmail(kFakePrimaryUsername)))));
    profile_builder.SetProfileName(kFakePrimaryUsername);
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(profile_builder);
    auto* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    account_manager_ = factory->GetAccountManager(profile()->GetPath().value());
    account_manager_facade_ =
        ::GetAccountManagerFacade(profile()->GetPath().value());
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    // Prep the Primary account.
    auto* identity_test_env =
        identity_test_environment_adaptor_->identity_test_env();
    const AccountInfo primary_account_info =
        identity_test_env->MakePrimaryAccountAvailable(
            kFakePrimaryUsername, signin::ConsentLevel::kSignin);
    primary_account_id_ = AccountId::FromUserEmailGaiaId(
        primary_account_info.email, primary_account_info.gaia);
    fake_user_manager_->AddUser(primary_account_id_);
    fake_user_manager_->LoginUser(primary_account_id_);

    // Add accounts in Account Manager.
    account_manager_->UpsertAccount(
        ::account_manager::AccountKey{primary_account_info.gaia,
                                      account_manager::AccountType::kGaia},
        primary_account_info.email,
        account_manager::AccountManager::kInvalidToken);
    account_manager_->UpsertAccount(
        ::account_manager::AccountKey{kFakeSecondaryGaiaId,
                                      account_manager::AccountType::kGaia},
        kFakeSecondaryUsername, account_manager::AccountManager::kInvalidToken);

    AccountManagerPolicyControllerFactory::GetForBrowserContext(profile());
  }

  void TearDownOnMainThread() override {
    fake_user_manager_->RemoveUserFromList(primary_account_id_);
    identity_test_environment_adaptor_.reset();
    profile_.reset();
    base::RunLoop().RunUntilIdle();
    fake_user_manager_.Reset();
  }

  std::vector<::account_manager::Account> GetAccountManagerAccounts() {
    CHECK(account_manager_facade_);

    base::test::TestFuture<const std::vector<::account_manager::Account>&>
        future;
    account_manager_facade_->GetAccounts(future.GetCallback());
    return future.Get();
  }

  Profile* profile() { return profile_.get(); }

  signin::IdentityManager* identity_manager() {
    return identity_test_environment_adaptor_->identity_test_env()
        ->identity_manager();
  }

 private:
  base::ScopedTempDir temp_dir_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  raw_ptr<account_manager::AccountManager, DanglingUntriaged> account_manager_ =
      nullptr;
  raw_ptr<account_manager::AccountManagerFacade> account_manager_facade_ =
      nullptr;
  std::unique_ptr<Profile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  AccountId primary_account_id_;
};

IN_PROC_BROWSER_TEST_F(AccountManagerPolicyControllerTest,
                       ExistingSecondaryAccountsAreNotRemovedIfPolicyIsNotSet) {
  std::vector<::account_manager::Account> accounts =
      GetAccountManagerAccounts();
  // We should have at least 1 Secondary Account.
  const std::vector<::account_manager::Account>::size_type
      initial_num_accounts = accounts.size();
  ASSERT_GT(initial_num_accounts, 1UL);

  // Use default policy value for |kSecondaryGoogleAccountSigninAllowed|
  // (|true|).
  profile()->GetPrefs()->SetBoolean(
      ::account_manager::prefs::kSecondaryGoogleAccountSigninAllowed, true);
  ChildAccountTypeChangedUserData::GetForProfile(profile())->SetValue(false);

  base::RunLoop().RunUntilIdle();

  // All accounts must be intact.
  accounts = GetAccountManagerAccounts();
  EXPECT_EQ(initial_num_accounts, accounts.size());
}

IN_PROC_BROWSER_TEST_F(
    AccountManagerPolicyControllerTest,
    ExistingSecondaryAccountsAreRemovedAfterPolicyApplication) {
  std::vector<::account_manager::Account> accounts =
      GetAccountManagerAccounts();
  // We should have at least 1 Secondary Account.
  ASSERT_GT(accounts.size(), 1UL);

  // Disallow secondary account sign-ins.
  profile()->GetPrefs()->SetBoolean(
      ::account_manager::prefs::kSecondaryGoogleAccountSigninAllowed, false);

  base::RunLoop().RunUntilIdle();

  // Secondary Accounts must be removed.
  const std::string gaia_id = BrowserContextHelper::Get()
                                  ->GetUserByBrowserContext(profile())
                                  ->GetAccountId()
                                  .GetGaiaId();
  accounts = GetAccountManagerAccounts();
  ASSERT_EQ(accounts.size(), 1UL);
  EXPECT_EQ(gaia_id, accounts[0].key.id());
  EXPECT_EQ(gaia_id, identity_manager()
                         ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                         .gaia);
}

IN_PROC_BROWSER_TEST_F(
    AccountManagerPolicyControllerTest,
    SecondaryAccountsAreRemovedAfterAccountTypeChangedWithCoexistenceEnabled) {
  std::vector<::account_manager::Account> accounts =
      GetAccountManagerAccounts();
  const std::vector<::account_manager::Account>::size_type
      initial_num_accounts = accounts.size();
  // We should have at least 1 Secondary Account.
  ASSERT_GT(initial_num_accounts, 1UL);

  // Disallow secondary account sign-ins.
  ChildAccountTypeChangedUserData::GetForProfile(profile())->SetValue(true);

  base::RunLoop().RunUntilIdle();

  // Secondary Accounts must be removed.
  const std::string gaia_id = BrowserContextHelper::Get()
                                  ->GetUserByBrowserContext(profile())
                                  ->GetAccountId()
                                  .GetGaiaId();
  accounts = GetAccountManagerAccounts();
  ASSERT_EQ(accounts.size(), 1UL);
  EXPECT_EQ(gaia_id, accounts[0].key.id());
  EXPECT_EQ(gaia_id, identity_manager()
                         ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                         .gaia);
}

}  // namespace ash
