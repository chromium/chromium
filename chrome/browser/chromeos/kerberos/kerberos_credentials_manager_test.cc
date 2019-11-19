// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/kerberos/kerberos_client.h"
#include "chromeos/dbus/kerberos/kerberos_service.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kProfileEmail[] = "gaia_user@example.com";
constexpr char kPrincipal[] = "kerbeROS_user@examPLE.com";
constexpr char kNormalizedPrincipal[] = "kerberos_user@EXAMPLE.COM";
constexpr char kOtherPrincipal[] = "icebear_cloud@example.com";
constexpr char kNormalizedOtherPrincipal[] = "icebear_cloud@EXAMPLE.COM";
constexpr char kYetAnotherPrincipal[] = "yet_another_user@example.com";
constexpr char kBadPrincipal1[] = "";
constexpr char kBadPrincipal2[] = "kerbeROS_user";
constexpr char kBadPrincipal3[] = "kerbeROS_user@";
constexpr char kBadPrincipal4[] = "@examPLE.com";
constexpr char kBadPrincipal5[] = "kerbeROS@user@examPLE.com";
constexpr char kPassword[] = "m1sst1ped>_<";
constexpr char kInvalidPassword[] = "";
constexpr char kConfig[] = "[libdefaults]";
constexpr char kInvalidConfig[] = "[libdefaults]\n  allow_weak_crypto = true";

const bool kUnmanaged = false;
const bool kManaged = true;

const bool kDontRememberPassword = false;
const bool kRememberPassword = true;

const bool kDontAllowExisting = false;
const bool kAllowExisting = true;

const int kNoNotification = 0;
const int kOneNotification = 1;

const int kNoAccount = 0;
const int kOneAccount = 1;
const int kTwoAccounts = 2;
const int kThreeAccounts = 3;

// Fake observer used to test notifications sent by KerberosCredentialsManager
// on accounts changes.
class FakeKerberosCredentialsManagerObserver
    : public KerberosCredentialsManager::Observer {
 public:
  FakeKerberosCredentialsManagerObserver() = default;
  ~FakeKerberosCredentialsManagerObserver() override = default;

  int notifications_count() const { return notifications_count_; }

  int accounts_count_at_last_notification() const {
    return accounts_count_at_last_notification_;
  }

  // KerberosCredentialsManager::Observer:
  void OnAccountsChanged() override {
    notifications_count_++;
    accounts_count_at_last_notification_ =
        KerberosClient::Get()->GetTestInterface()->GetNumberOfAccounts();
  }

  void Reset() {
    notifications_count_ = 0;
    accounts_count_at_last_notification_ = 0;
  }

 private:
  // Registers how many times the observer has been notified of account changes.
  int notifications_count_ = 0;
  // Registers the number of accounts saved before the most recent
  // OnAccountsChanged() call.
  int accounts_count_at_last_notification_ = 0;
};
}  // namespace

class KerberosCredentialsManagerTest : public testing::Test {
 public:
  using Account = kerberos::Account;
  using Accounts = std::vector<Account>;

  KerberosCredentialsManagerTest()
      : scoped_user_manager_(
            std::make_unique<testing::NiceMock<MockUserManager>>()),
        local_state_(TestingBrowserProcess::GetGlobal()) {
    KerberosClient::InitializeFake();
    client_test_interface()->SetTaskDelay(base::TimeDelta());

    mock_user_manager()->AddUser(AccountId::FromUserEmail(kProfileEmail));

    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(kProfileEmail);
    profile_ = profile_builder.Build();

    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_.get());

    mgr_ = std::make_unique<KerberosCredentialsManager>(local_state_.Get(),
                                                        profile_.get());

    mgr_->AddObserver(&observer_);
  }

  ~KerberosCredentialsManagerTest() override {
    mgr_->RemoveObserver(&observer_);
    mgr_.reset();
    display_service_.reset();
    profile_.reset();
    KerberosClient::Shutdown();
  }

  void SetPref(const char* name, base::Value value) {
    local_state_.Get()->SetManagedPref(
        prefs::kKerberosEnabled,
        std::make_unique<base::Value>(std::move(value)));
  }

 protected:
  MockUserManager* mock_user_manager() {
    return static_cast<MockUserManager*>(user_manager::UserManager::Get());
  }

  KerberosClient::TestInterface* client_test_interface() {
    return KerberosClient::Get()->GetTestInterface();
  }

  // Gets a callback that adds the passed-in error to |result_errors_|.
  KerberosCredentialsManager::ResultCallback GetResultCallback() {
    // If this is the first account addition, sets |result_run_loop_|.
    if (accounts_addition_count_ == 0) {
      EXPECT_TRUE(result_errors_.empty());
      EXPECT_FALSE(result_run_loop_);
      result_run_loop_ = std::make_unique<base::RunLoop>();
    }
    accounts_addition_count_++;

    return base::BindOnce(&KerberosCredentialsManagerTest::OnResult,
                          weak_ptr_factory_.GetWeakPtr());
  }

  void OnResult(kerberos::ErrorType error) {
    DCHECK_LT(0, accounts_addition_count_);
    accounts_addition_count_--;
    result_errors_.insert(error);

    // Stops |result_run_loop_| if all additions are finished.
    if (accounts_addition_count_ == 0) {
      result_run_loop_->Quit();
    }
  }

  void WaitAndVerifyResult(std::multiset<kerberos::ErrorType> expected_errors_,
                           int expected_notifications_count,
                           int expected_accounts_count) {
    EXPECT_LT(0, accounts_addition_count_);
    ASSERT_TRUE(result_run_loop_);
    result_run_loop_->Run();

    EXPECT_EQ(expected_errors_, result_errors_);
    EXPECT_EQ(expected_notifications_count, observer_.notifications_count());
    EXPECT_EQ(expected_accounts_count,
              observer_.accounts_count_at_last_notification());

    EXPECT_EQ(0, accounts_addition_count_);
    result_run_loop_.reset();
    result_errors_.clear();
    observer_.Reset();
  }

  // Calls |mgr_->AddAccountAndAuthenticate()| with |principal_name| and some
  // default parameters, waits for the result and checks expectations.
  void AddAccountAndAuthenticate(const char* principal_name,
                                 kerberos::ErrorType expected_error,
                                 int expected_notifications_count,
                                 int expected_accounts_count) {
    mgr_->AddAccountAndAuthenticate(principal_name, kUnmanaged, kPassword,
                                    kDontRememberPassword, kConfig,
                                    kAllowExisting, GetResultCallback());
    WaitAndVerifyResult({expected_error}, expected_notifications_count,
                        expected_accounts_count);
  }

  // Calls |mgr_->ListAccounts()|, waits for the result and expects success.
  // Returns the list of accounts.
  Accounts ListAccounts() {
    base::RunLoop run_loop;
    Accounts accounts;
    mgr_->ListAccounts(base::BindLambdaForTesting(
        [&](const kerberos::ListAccountsResponse& response) {
          EXPECT_EQ(kerberos::ERROR_NONE, response.error());
          for (int n = 0; n < response.accounts_size(); ++n)
            accounts.push_back(std::move(response.accounts(n)));
          run_loop.Quit();
        }));
    run_loop.Run();
    return accounts;
  }

  // Similar to ListAccounts(), but expects exactly 1 account and returns it.
  // Returns a default account if none exists.
  kerberos::Account GetAccount() {
    Accounts accounts = ListAccounts();
    EXPECT_LE(1u, accounts.size());
    if (accounts.size() != 1)
      return Account();
    return std::move(accounts[0]);
  }

  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager scoped_user_manager_;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  std::unique_ptr<KerberosCredentialsManager> mgr_;
  FakeKerberosCredentialsManagerObserver observer_;

  int accounts_addition_count_ = 0;
  std::unique_ptr<base::RunLoop> result_run_loop_;
  std::multiset<kerberos::ErrorType> result_errors_;

 private:
  base::WeakPtrFactory<KerberosCredentialsManagerTest> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(KerberosCredentialsManagerTest);
};

// The default config sets strong crypto and allows forwardable tickets.
TEST_F(KerberosCredentialsManagerTest, GetDefaultKerberosConfig) {
  const std::string default_config = mgr_->GetDefaultKerberosConfig();

  // Enforce strong crypto.
  EXPECT_TRUE(base::Contains(default_config, "default_tgs_enctypes"));
  EXPECT_TRUE(base::Contains(default_config, "default_tkt_enctypes"));
  EXPECT_TRUE(base::Contains(default_config, "permitted_enctypes"));
  EXPECT_TRUE(base::Contains(default_config, "aes256"));
  EXPECT_TRUE(base::Contains(default_config, "aes128"));
  EXPECT_FALSE(base::Contains(default_config, "des"));
  EXPECT_FALSE(base::Contains(default_config, "rc4"));

  // Allow forwardable tickets.
  EXPECT_TRUE(base::Contains(default_config, "forwardable = true"));
}

// The prefs::kKerberosEnabled pref toggles IsKerberosEnabled().
TEST_F(KerberosCredentialsManagerTest, IsKerberosEnabled) {
  EXPECT_FALSE(mgr_->IsKerberosEnabled());
  SetPref(prefs::kKerberosEnabled, base::Value(true));
  EXPECT_TRUE(mgr_->IsKerberosEnabled());
}

// AddAccountAndAuthenticate() successfully adds an account. The principal name
// is normalized.
TEST_F(KerberosCredentialsManagerTest,
       AddAccountAndAuthenticateNormalizesPrincipal) {
  mgr_->AddAccountAndAuthenticate(kPrincipal, kManaged, kPassword,
                                  kRememberPassword, kConfig,
                                  kDontAllowExisting, GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_NONE}, kOneNotification, kOneAccount);

  Account account = GetAccount();
  EXPECT_EQ(kNormalizedPrincipal, account.principal_name());
  EXPECT_EQ(kRememberPassword, account.password_was_remembered());
  EXPECT_EQ(kManaged, account.is_managed());
}

// AddAccountAndAuthenticate() fails with ERROR_PARSE_PRINCIPAL_FAILED when a
// bad principal name is passed in.
TEST_F(KerberosCredentialsManagerTest,
       AddAccountAndAuthenticateFailsForBadPrincipal) {
  const kerberos::ErrorType expected_error =
      kerberos::ERROR_PARSE_PRINCIPAL_FAILED;
  AddAccountAndAuthenticate(kBadPrincipal1, expected_error, kNoNotification,
                            kNoAccount);
  AddAccountAndAuthenticate(kBadPrincipal2, expected_error, kNoNotification,
                            kNoAccount);
  AddAccountAndAuthenticate(kBadPrincipal3, expected_error, kNoNotification,
                            kNoAccount);
  AddAccountAndAuthenticate(kBadPrincipal4, expected_error, kNoNotification,
                            kNoAccount);
  AddAccountAndAuthenticate(kBadPrincipal5, expected_error, kNoNotification,
                            kNoAccount);
}

// AddAccountAndAuthenticate calls KerberosClient methods in a certain order.
TEST_F(KerberosCredentialsManagerTest,
       AddAccountAndAuthenticateCallsKerberosClient) {
  // Specifying password includes AcquireKerberosTgt() call.
  client_test_interface()->StartRecordingFunctionCalls();
  mgr_->AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kPassword,
                                  kDontRememberPassword, kConfig,
                                  kAllowExisting, GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_NONE}, kOneNotification, kOneAccount);
  std::string calls =
      client_test_interface()->StopRecordingAndGetRecordedFunctionCalls();
  EXPECT_EQ(calls, "AddAccount,SetConfig,AcquireKerberosTgt,GetKerberosFiles");

  // Specifying no password excludes AcquireKerberosTgt() call.
  const base::Optional<std::string> kNoPassword;
  client_test_interface()->StartRecordingFunctionCalls();
  mgr_->AddAccountAndAuthenticate(kPrincipal, kManaged, kNoPassword,
                                  kDontRememberPassword, kConfig,
                                  kAllowExisting, GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_NONE}, kOneNotification, kOneAccount);
  calls = client_test_interface()->StopRecordingAndGetRecordedFunctionCalls();
  EXPECT_EQ(calls, "AddAccount,SetConfig,GetKerberosFiles");
}

// AddAccountAndAuthenticate rejects existing accounts with
// ERROR_DUPLICATE_PRINCIPAL_NAME if |kDontAllowExisting| is passed in.
TEST_F(KerberosCredentialsManagerTest,
       AddAccountAndAuthenticateRejectExistingAccount) {
  AddAccountAndAuthenticate(kPrincipal, kerberos::ERROR_NONE, kOneNotification,
                            kOneAccount);
  mgr_->AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kPassword,
                                  kRememberPassword, kConfig,
                                  kDontAllowExisting, GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_DUPLICATE_PRINCIPAL_NAME},
                      kOneNotification, kOneAccount);
}

// AddAccountAndAuthenticate succeeds and overwrites existing accounts if
// |kAllowExisting| is passed in.
TEST_F(KerberosCredentialsManagerTest,
       AddAccountAndAuthenticateAllowExistingAccount) {
  AddAccountAndAuthenticate(kPrincipal, kerberos::ERROR_NONE, kOneNotification,
                            kOneAccount);
  EXPECT_FALSE(GetAccount().password_was_remembered());

  mgr_->AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kPassword,
                                  kRememberPassword, kConfig, kAllowExisting,
                                  GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_NONE}, kOneNotification, kOneAccount);

  // Check change in password_was_remembered() to validate that the account was
  // overwritten.
  EXPECT_TRUE(GetAccount().password_was_remembered());
}

// AddAccountAndAuthenticate removes accounts added in the same call on failure.
TEST_F(KerberosCredentialsManagerTest,
       AddAccountAndAuthenticateRemovesAccountOnFailure) {
  // Setting an invalid config causes ERROR_BAD_CONFIG.
  // The previously added account is removed again.
  client_test_interface()->StartRecordingFunctionCalls();
  mgr_->AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kPassword,
                                  kRememberPassword, kInvalidConfig,
                                  kAllowExisting, GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_BAD_CONFIG}, kOneNotification,
                      kNoAccount);
  std::string calls =
      client_test_interface()->StopRecordingAndGetRecordedFunctionCalls();
  EXPECT_EQ(calls, "AddAccount,SetConfig,RemoveAccount");
  EXPECT_EQ(0u, ListAccounts().size());

  // Likewise, setting an invalid password (empty string) causes
  // ERROR_BAD_PASSWORD and the previously added account is removed again.
  client_test_interface()->StartRecordingFunctionCalls();
  mgr_->AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kInvalidPassword,
                                  kRememberPassword, kConfig, kAllowExisting,
                                  GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_BAD_PASSWORD}, kOneNotification,
                      kNoAccount);
  calls = client_test_interface()->StopRecordingAndGetRecordedFunctionCalls();
  EXPECT_EQ(calls, "AddAccount,SetConfig,AcquireKerberosTgt,RemoveAccount");
  EXPECT_EQ(0u, ListAccounts().size());
}

// AddAccountAndAuthenticate does not remove accounts that already existed.
TEST_F(KerberosCredentialsManagerTest,
       AddAccountAndAuthenticateDoesNotRemoveExistingAccountOnFailure) {
  AddAccountAndAuthenticate(kPrincipal, kerberos::ERROR_NONE, kOneNotification,
                            kOneAccount);

  client_test_interface()->StartRecordingFunctionCalls();
  mgr_->AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kPassword,
                                  kRememberPassword, kInvalidConfig,
                                  kAllowExisting, GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_BAD_CONFIG}, kOneNotification,
                      kOneAccount);
  std::string calls =
      client_test_interface()->StopRecordingAndGetRecordedFunctionCalls();
  EXPECT_EQ(calls, "AddAccount,SetConfig");
  EXPECT_EQ(1u, ListAccounts().size());
}

// AddAccountAndAuthenticate does not remove managed accounts.
TEST_F(KerberosCredentialsManagerTest,
       AddAccountAndAuthenticateDoesNotRemoveManagedAccountOnFailure) {
  // Setting an invalid config causes ERROR_BAD_CONFIG.
  // The previously added account is removed again.
  client_test_interface()->StartRecordingFunctionCalls();
  mgr_->AddAccountAndAuthenticate(kPrincipal, kManaged, kPassword,
                                  kRememberPassword, kInvalidConfig,
                                  kAllowExisting, GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_BAD_CONFIG}, kOneNotification,
                      kOneAccount);
  std::string calls =
      client_test_interface()->StopRecordingAndGetRecordedFunctionCalls();
  EXPECT_EQ(calls, "AddAccount,SetConfig");
  EXPECT_EQ(1u, ListAccounts().size());
}

// AddAccountAndAuthenticate sets the active account for every UNMANAGED account
// added.
TEST_F(KerberosCredentialsManagerTest,
       AddAccountAndAuthenticateSetsActiveAccountUnmanaged) {
  // Adding an unmanaged account with no active account sets the active account.
  EXPECT_TRUE(mgr_->GetActiveAccount().empty());
  mgr_->AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kPassword,
                                  kDontRememberPassword, kConfig,
                                  kAllowExisting, GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_NONE}, kOneNotification, kOneAccount);
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());

  // Adding another unmanaged account DOES change the active account.
  mgr_->AddAccountAndAuthenticate(kOtherPrincipal, kUnmanaged, kPassword,
                                  kDontRememberPassword, kConfig,
                                  kAllowExisting, GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_NONE}, kOneNotification, kTwoAccounts);
  EXPECT_EQ(kNormalizedOtherPrincipal, mgr_->GetActiveAccount());
}

// AddAccountAndAuthenticate sets the active account only if there was no active
// account if a MANAGED account is added.
TEST_F(KerberosCredentialsManagerTest,
       AddAccountAndAuthenticateSetsActiveAccountManaged) {
  // Adding a managed account with no active account sets the active account.
  EXPECT_TRUE(mgr_->GetActiveAccount().empty());
  mgr_->AddAccountAndAuthenticate(kPrincipal, kManaged, kPassword,
                                  kDontRememberPassword, kConfig,
                                  kAllowExisting, GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_NONE}, kOneNotification, kOneAccount);
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());

  // Adding another managed account DOES NOT change the active account.
  mgr_->AddAccountAndAuthenticate(kOtherPrincipal, kManaged, kPassword,
                                  kDontRememberPassword, kConfig,
                                  kAllowExisting, GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_NONE}, kOneNotification, kTwoAccounts);
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
}

// AddAccountAndAuthenticate attempts to add multiple accounts, all of them
// fail, and observers are notified once.
TEST_F(KerberosCredentialsManagerTest,
       AddAccountAndAuthenticateAddsMultipleAccountsAllFail) {
  EXPECT_TRUE(mgr_->GetActiveAccount().empty());
  mgr_->AddAccountAndAuthenticate(kPrincipal, kManaged, kInvalidPassword,
                                  kDontRememberPassword, kConfig,
                                  kAllowExisting, GetResultCallback());
  mgr_->AddAccountAndAuthenticate(kOtherPrincipal, kManaged, kInvalidPassword,
                                  kDontRememberPassword, kConfig,
                                  kAllowExisting, GetResultCallback());
  mgr_->AddAccountAndAuthenticate(kYetAnotherPrincipal, kManaged,
                                  kInvalidPassword, kDontRememberPassword,
                                  kConfig, kAllowExisting, GetResultCallback());

  WaitAndVerifyResult(
      {kerberos::ERROR_BAD_PASSWORD, kerberos::ERROR_BAD_PASSWORD,
       kerberos::ERROR_BAD_PASSWORD},
      kOneNotification, kThreeAccounts);
  EXPECT_TRUE(mgr_->GetActiveAccount().empty());
}

// AddAccountAndAuthenticate attempts to add multiple accounts, all of them
// succeed, and observers are notified once.
TEST_F(KerberosCredentialsManagerTest,
       AddAccountAndAuthenticateAddsMultipleAccountsAllSucceed) {
  EXPECT_TRUE(mgr_->GetActiveAccount().empty());
  mgr_->AddAccountAndAuthenticate(kPrincipal, kManaged, kPassword,
                                  kDontRememberPassword, kConfig,
                                  kAllowExisting, GetResultCallback());
  mgr_->AddAccountAndAuthenticate(kOtherPrincipal, kManaged, kPassword,
                                  kDontRememberPassword, kConfig,
                                  kAllowExisting, GetResultCallback());
  mgr_->AddAccountAndAuthenticate(kYetAnotherPrincipal, kManaged, kPassword,
                                  kDontRememberPassword, kConfig,
                                  kAllowExisting, GetResultCallback());

  WaitAndVerifyResult(
      {kerberos::ERROR_NONE, kerberos::ERROR_NONE, kerberos::ERROR_NONE},
      kOneNotification, kThreeAccounts);
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
}

// AddAccountAndAuthenticate attempts to add multiple accounts, only one
// succeed, and observers are notified once.
TEST_F(KerberosCredentialsManagerTest,
       AddAccountAndAuthenticateAddsMultipleAccountsSingleSuccess) {
  EXPECT_TRUE(mgr_->GetActiveAccount().empty());
  mgr_->AddAccountAndAuthenticate(kPrincipal, kManaged, kInvalidPassword,
                                  kDontRememberPassword, kConfig,
                                  kAllowExisting, GetResultCallback());
  mgr_->AddAccountAndAuthenticate(kOtherPrincipal, kManaged, kPassword,
                                  kDontRememberPassword, kConfig,
                                  kAllowExisting, GetResultCallback());
  mgr_->AddAccountAndAuthenticate(kYetAnotherPrincipal, kManaged,
                                  kInvalidPassword, kDontRememberPassword,
                                  kConfig, kAllowExisting, GetResultCallback());

  WaitAndVerifyResult({kerberos::ERROR_BAD_PASSWORD, kerberos::ERROR_NONE,
                       kerberos::ERROR_BAD_PASSWORD},
                      kOneNotification, kThreeAccounts);
  EXPECT_EQ(kNormalizedOtherPrincipal, mgr_->GetActiveAccount());
}

// TODO(https://crbug.com/952251): Add more tests
// - RemoveAccount
//     + Normalization like in AddAccountAndAuthenticate
//     + Calls the RemoveAccount KerberosClient method
//     + On success and if active principal was removed, sets a new active
//       principal (empty if no accounts left)
//     + On success, calls OnAccountsChanged on observers
// - ClearAccounts
//     + Normalization like in AddAccountAndAuthenticate
//     + Calls the ClearAccounts KerberosClient method
//     + If CLEAR_ALL, CLEAR_ONLY_*_ACCOUNTS: Reassigns active principal if it
//       was deleted.
//     + On success, calls OnAccountsChanged on observers
// - ListAccounts
//     + Normalization like in AddAccountAndAuthenticate
//     + Calls the ListAccounts KerberosClient method
//     + Reassigns an active principal if it was empty
// - SetConfig
//     + Normalization like in AddAccountAndAuthenticate
//     + Calls the SetConfig KerberosClient method
//     + On success, calls OnAccountsChanged on observers
// - ValidateConfig
//     + Normalization like in AddAccountAndAuthenticate
//     + Calls the ValidateConfig KerberosClient method
// - AcquireKerberosTgt
//     + Normalization like in AddAccountAndAuthenticate
//     + Calls the AcquireKerberosTgt KerberosClient method
// - SetActiveAccount
//     + Calls OnAccountsChanged on observers
// - GetKerberosFiles
//     + Earlies out if the active principal is empty
//     + Calls the GetKerberosFiles KerberosClient method
//     + Does nothing if active principal changed during the async call
//     + On success, calls kerberos_files_handler_.SetFiles if there's a
//     krb5cc
//     + On success, calls kerberos_files_handler_.DeleteFiles otherwise
// - OnKerberosFilesChanged
//     + Gets called when KerberosClient fires the KerberosFilesChanged signal
//     + Calls GetKerberosFiles if the active principal matches.
// - OnKerberosTicketExpiring
//     + Gets called when KerberosClient fires the KerberosTicketExpiring
//     signal
//     + Calls kerberos_ticket_expiry_notification::Show() if the active
//       principal matches.
// - UpdateEnabledFromPref()
//     + Gets called then prefs::KerberosEnabled changes.
//     + If it's switched off, all accounts are wiped.
//     + If it's switched on, managed accounts are restored
//       (see UpdateAccountsFromPref())
// - UpdateRememberPasswordEnabledFromPref()
//     + Gets called then prefs::kKerberosRememberPasswordEnabled changes.
//     + If it's switched off, all remembered unmanaged passwords are removed.
// - UpdateAddAccountsAllowedFromPref()
//     + Gets called then prefs::kKerberosAddAccountsAllowed changes.
//     + If it's switched off, all unmanaged accounts are removed.
// - UpdateAccountsFromPref()
//     + Gets called then prefs::kKerberosAccounts changes.
//     + If Kerberos is disabled, calls VoteForSavingLoginPassword(false)
//     + If there are no accounts, calls VoteForSavingLoginPassword(false)
//     + Accounts with bad principals ("${LOGIN_ID", "user@example@com") are
//       ignored
//     + Uses config if given and default config if not
//     + Calls VoteForSavingLoginPassword(res), where
//       res = any(account has password="${PASSWORD}")
//     + Clears out old managed accounts not in prefs::kKerberosAccounts
//     anymore
// - Notification
//     + If the notification is clicked, shows the KerberosAccounts settings
//       with ?kerberos_reauth=<principal name>
// - If policy service finishes initialization after constructor of
//   KerberosCredentialsManager, UpdateAccountsFromPref is called.
//
// See also
//   https://analysis.chromium.org/p/chromium/coverage/dir?host=chromium.googlesource.com&project=chromium/src&ref=refs/heads/master&revision=8e25360b5986bc807eb05927b59cb698b120140c&path=//chrome/browser/chromeos/kerberos/&platform=linux-chromeos
// for code coverage (try to get as high as possible!).

}  // namespace chromeos
