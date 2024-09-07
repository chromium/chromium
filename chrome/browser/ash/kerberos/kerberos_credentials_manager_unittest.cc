// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/kerberos/kerberos_files_handler.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/kerberos/kerberos_client.h"
#include "chromeos/ash/components/dbus/kerberos/kerberos_service.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

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
constexpr char kBadManagedPrincipal[] = "${LOGIN_ID@examPLE.com";
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

const bool kDontSaveLoginPassword = false;
const bool kSaveLoginPassword = true;

const int kNoNotification = 0;
const int kOneNotification = 1;
const int kTwoNotifications = 2;

const int kNoAccount = 0;
const int kOneAccount = 1;
const int kTwoAccounts = 2;
const int kThreeAccounts = 3;

const int kOneFailure = 1;
const int kThreeFailures = 3;
const int kLotsOfFailures = 1000000;

// Account keys for the kerberos.accounts pref.
constexpr char kKeyPrincipal[] = "principal";
constexpr char kKeyPassword[] = "password";
constexpr char kKeyRememberPasswordFromPolicy[] =
    "remember_password_from_policy";
constexpr char kKeyKrb5Conf[] = "krb5conf";

// Password placeholder.
constexpr char kLoginPasswordPlaceholder[] = "${PASSWORD}";

// A long time delta, used to fast forward the task environment until all
// pending operations are completed. This value should be equal to the maximum
// time to delay requests on |kBackoffPolicyForManagedAccounts|.
const base::TimeDelta kLongTimeDelay = base::Minutes(10);

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

class MockKerberosFilesHandler : public KerberosFilesHandler {
 public:
  explicit MockKerberosFilesHandler(base::RepeatingClosure get_kerberos_files)
      : KerberosFilesHandler(get_kerberos_files) {}

  ~MockKerberosFilesHandler() override = default;

  MOCK_METHOD(void, DeleteFiles, ());
};

}  // namespace

class KerberosCredentialsManagerTest : public testing::Test {
 public:
  using Account = kerberos::Account;
  using Accounts = std::vector<Account>;

  KerberosCredentialsManagerTest()
      : scoped_user_manager_(std::make_unique<FakeChromeUserManager>()),
        local_state_(TestingBrowserProcess::GetGlobal()) {
    SessionManagerClient::InitializeFakeInMemory();
    KerberosClient::InitializeFake();
    client_test_interface()->SetTaskDelay(base::TimeDelta());

    fake_user_manager()->AddUser(AccountId::FromUserEmail(kProfileEmail));

    // Setting the login password for the KerberosAccounts policy tests.
    UserContext* user_context =
        UserSessionManager::GetInstance()->mutable_user_context_for_testing();
    user_context->SetPasswordKey(Key(kPassword));
    UserSessionManager::GetInstance()->set_start_session_type_for_testing(
        UserSessionManager::StartSessionType::kPrimary);

    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(kProfileEmail);
    profile_ = profile_builder.Build();

    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_.get());

    mgr_ = std::make_unique<KerberosCredentialsManager>(local_state_.Get(),
                                                        profile_.get());

    mgr_->AddObserver(&observer_);
  }

  KerberosCredentialsManagerTest(const KerberosCredentialsManagerTest&) =
      delete;
  KerberosCredentialsManagerTest& operator=(
      const KerberosCredentialsManagerTest&) = delete;

  ~KerberosCredentialsManagerTest() override {
    mgr_->RemoveObserver(&observer_);
    mgr_.reset();
    display_service_.reset();
    profile_.reset();
    UserSessionManager::GetInstance()->Shutdown();
    KerberosClient::Shutdown();
    SessionManagerClient::Shutdown();
  }

  void SetPref(const char* name, base::Value value) {
    local_state_.Get()->SetManagedPref(
        name, std::make_unique<base::Value>(std::move(value)));
  }

 protected:
  FakeChromeUserManager* fake_user_manager() {
    return static_cast<FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  KerberosClient::TestInterface* client_test_interface() {
    return KerberosClient::Get()->GetTestInterface();
  }

  void SetupResultCallback(int operation_count) {
    // If this is the first account addition, sets |result_run_loop_|.
    if (remaining_operation_count_ == 0) {
      EXPECT_TRUE(result_errors_.empty());
      EXPECT_FALSE(result_run_loop_);
      result_run_loop_ = std::make_unique<base::RunLoop>();
    }

    remaining_operation_count_ += operation_count;
  }

  // Gets a callback that adds the passed-in error to |result_errors_|.
  // |operation_count| is the number of times the callback should be called
  // before stopping |result_run_loop_|.
  base::RepeatingCallback<void(kerberos::ErrorType)> GetRepeatingCallback(
      int operation_count) {
    SetupResultCallback(operation_count);
    return base::BindRepeating(&KerberosCredentialsManagerTest::OnResult,
                               weak_ptr_factory_.GetWeakPtr());
  }

  // Gets a callback that adds the passed-in error to |result_errors_|.
  KerberosCredentialsManager::ResultCallback GetResultCallback() {
    SetupResultCallback(1);
    return base::BindOnce(&KerberosCredentialsManagerTest::OnResult,
                          weak_ptr_factory_.GetWeakPtr());
  }

  void OnResult(kerberos::ErrorType error) {
    // Fails if the test tries to execute more operations than expected.
    ASSERT_LT(0, remaining_operation_count_);
    remaining_operation_count_--;
    result_errors_.insert(error);

    // Stops |result_run_loop_| if all additions are finished.
    if (remaining_operation_count_ == 0) {
      result_run_loop_->Quit();
    }
  }

  void WaitAndVerifyResult(std::multiset<kerberos::ErrorType> expected_errors,
                           int expected_notifications_count,
                           int expected_accounts_count) {
    EXPECT_LT(0, remaining_operation_count_);
    ASSERT_TRUE(result_run_loop_);
    result_run_loop_->Run();

    EXPECT_EQ(expected_errors, result_errors_);
    EXPECT_EQ(expected_notifications_count, observer_.notifications_count());
    EXPECT_EQ(expected_accounts_count,
              observer_.accounts_count_at_last_notification());

    EXPECT_EQ(0, remaining_operation_count_);
    result_run_loop_.reset();
    result_errors_.clear();
    observer_.Reset();
  }

  std::multiset<kerberos::ErrorType> GetRepeatedError(kerberos::ErrorType error,
                                                      int repetitions) {
    std::multiset<kerberos::ErrorType> result;
    for (int i = 0; i < repetitions; i++) {
      result.insert(error);
    }

    return result;
  }

  // Calls |mgr_->AddAccountAndAuthenticate()| with |principal_name|,
  // |is_managed| and some default parameters, waits for the result and checks
  // expectations.
  void AddAccountAndAuthenticate(const char* principal_name,
                                 bool is_managed,
                                 kerberos::ErrorType expected_error,
                                 int expected_notifications_count,
                                 int expected_accounts_count) {
    mgr_->AddAccountAndAuthenticate(principal_name, is_managed, kPassword,
                                    kDontRememberPassword, kConfig,
                                    kAllowExisting, GetResultCallback());
    WaitAndVerifyResult({expected_error}, expected_notifications_count,
                        expected_accounts_count);
  }

  void RemoveAccount(const char* principal_name,
                     kerberos::ErrorType expected_error,
                     int expected_notifications_count,
                     int expected_accounts_count) {
    base::RunLoop run_loop;
    mgr_->RemoveAccount(
        principal_name,
        base::BindLambdaForTesting([&](const kerberos::ErrorType error) {
          EXPECT_EQ(expected_error, error);
          run_loop.Quit();
        }));
    run_loop.Run();

    EXPECT_EQ(expected_notifications_count, observer_.notifications_count());
    EXPECT_EQ(expected_accounts_count,
              observer_.accounts_count_at_last_notification());
    observer_.Reset();
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

  // Verify KerberosCredentialsManager voted for saving login password for
  // Kerberos service on UserSessionManager with value |save_login_password|.
  void VerifyVotedForSavingLoginPassword(bool save_login_password) {
    // Votes for not saving login password for all the other services. This way,
    // the password should be saved if and only if Kerberos service voted for
    // saving it.
    for (auto s = static_cast<UserSessionManager::PasswordConsumingService>(0);
         s < UserSessionManager::PasswordConsumingService::kCount;
         s = static_cast<UserSessionManager::PasswordConsumingService>(
             static_cast<int>(s) + 1)) {
      if (s != UserSessionManager::PasswordConsumingService::kKerberos) {
        UserSessionManager::GetInstance()->VoteForSavingLoginPassword(
            s, kDontSaveLoginPassword);
      }
    }

    // Checks whether the password was saved according to |save_login_password|.
    if (save_login_password == kSaveLoginPassword) {
      EXPECT_EQ(kPassword, FakeSessionManagerClient::Get()->login_password());
    } else {
      EXPECT_TRUE(FakeSessionManagerClient::Get()->login_password().empty());
    }

    // The password should have being deleted from |user_context| at the end.
    const UserContext& user_context =
        UserSessionManager::GetInstance()->user_context();
    EXPECT_TRUE(user_context.GetPasswordKey()->GetSecret().empty());
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  user_manager::ScopedUserManager scoped_user_manager_;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  std::unique_ptr<KerberosCredentialsManager> mgr_;
  FakeKerberosCredentialsManagerObserver observer_;

  int remaining_operation_count_ = 0;
  std::unique_ptr<base::RunLoop> result_run_loop_;
  std::multiset<kerberos::ErrorType> result_errors_;

 private:
  base::WeakPtrFactory<KerberosCredentialsManagerTest> weak_ptr_factory_{this};
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
  AddAccountAndAuthenticate(kBadPrincipal1, kUnmanaged, expected_error,
                            kNoNotification, kNoAccount);
  AddAccountAndAuthenticate(kBadPrincipal2, kUnmanaged, expected_error,
                            kNoNotification, kNoAccount);
  AddAccountAndAuthenticate(kBadPrincipal3, kUnmanaged, expected_error,
                            kNoNotification, kNoAccount);
  AddAccountAndAuthenticate(kBadPrincipal4, kUnmanaged, expected_error,
                            kNoNotification, kNoAccount);
  AddAccountAndAuthenticate(kBadPrincipal5, kUnmanaged, expected_error,
                            kNoNotification, kNoAccount);
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
  const std::optional<std::string> kNoPassword;
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
  AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kOneAccount);
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
  AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kOneAccount);
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
  AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kOneAccount);

  client_test_interface()->StartRecordingFunctionCalls();
  mgr_->AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kPassword,
                                  kRememberPassword, kInvalidConfig,
                                  kAllowExisting, GetResultCallback());
  WaitAndVerifyResult({kerberos::ERROR_BAD_CONFIG}, kOneNotification,
                      kOneAccount);
  const std::string calls =
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
  const std::string calls =
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
      GetRepeatedError(kerberos::ERROR_BAD_PASSWORD, kThreeAccounts),
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

  WaitAndVerifyResult(GetRepeatedError(kerberos::ERROR_NONE, kThreeAccounts),
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

// RemoveAccount fails with ERROR_PARSE_PRINCIPAL_FAILED when a bad principal
// name is passed in.
TEST_F(KerberosCredentialsManagerTest, RemoveAccountFailsForBadPrincipal) {
  const kerberos::ErrorType expected_error =
      kerberos::ERROR_PARSE_PRINCIPAL_FAILED;
  RemoveAccount(kBadPrincipal1, expected_error, kNoNotification, kNoAccount);
  RemoveAccount(kBadPrincipal2, expected_error, kNoNotification, kNoAccount);
  RemoveAccount(kBadPrincipal3, expected_error, kNoNotification, kNoAccount);
  RemoveAccount(kBadPrincipal4, expected_error, kNoNotification, kNoAccount);
  RemoveAccount(kBadPrincipal5, expected_error, kNoNotification, kNoAccount);
}

// RemoveAccount normalizes |principal_name| before trying to find account.
TEST_F(KerberosCredentialsManagerTest, RemoveAccountNormalizesPrincipalName) {
  AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kOneAccount);
  RemoveAccount(kPrincipal, kerberos::ERROR_NONE, kOneNotification, kNoAccount);
}

// RemoveAccount removes last account, should clear the active principal.
TEST_F(KerberosCredentialsManagerTest,
       RemoveAccountRemoveLastAccountClearsActivePrincipal) {
  AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kOneAccount);
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
  client_test_interface()->StartRecordingFunctionCalls();

  RemoveAccount(kPrincipal, kerberos::ERROR_NONE, kOneNotification, kNoAccount);

  const std::string calls =
      client_test_interface()->StopRecordingAndGetRecordedFunctionCalls();
  EXPECT_EQ(calls, "RemoveAccount");
  EXPECT_TRUE(mgr_->GetActiveAccount().empty());
}

// RemoveAccount removes last account, should delete kerberos files.
TEST_F(KerberosCredentialsManagerTest,
       RemoveAccountRemoveLastAccountDeletesKerberosFiles) {
  auto files_handler = std::make_unique<MockKerberosFilesHandler>(
      mgr_->GetGetKerberosFilesCallbackForTesting());
  EXPECT_CALL(*files_handler, DeleteFiles());
  mgr_->SetKerberosFilesHandlerForTesting(std::move(files_handler));
  AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kOneAccount);

  RemoveAccount(kPrincipal, kerberos::ERROR_NONE, kOneNotification, kNoAccount);
}

// RemoveAccount removes active account while another account is available,
// should set a new active principal.
TEST_F(KerberosCredentialsManagerTest,
       RemoveAccountRemoveActiveAccountAnotherAccountAvailable) {
  AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kOneAccount);
  AddAccountAndAuthenticate(kOtherPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kTwoAccounts);
  EXPECT_EQ(kNormalizedOtherPrincipal, mgr_->GetActiveAccount());
  client_test_interface()->StartRecordingFunctionCalls();

  RemoveAccount(kOtherPrincipal, kerberos::ERROR_NONE, kOneNotification,
                kOneAccount);

  const std::string calls =
      client_test_interface()->StopRecordingAndGetRecordedFunctionCalls();
  EXPECT_EQ(calls, "RemoveAccount,GetKerberosFiles");
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
}

// RemoveAccount removes non-active account, no change on active account.
TEST_F(KerberosCredentialsManagerTest, RemoveAccountNonActiveAccount) {
  AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kOneAccount);
  AddAccountAndAuthenticate(kOtherPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kTwoAccounts);
  EXPECT_EQ(kNormalizedOtherPrincipal, mgr_->GetActiveAccount());

  client_test_interface()->StartRecordingFunctionCalls();
  RemoveAccount(kPrincipal, kerberos::ERROR_NONE, kOneNotification,
                kOneAccount);

  const std::string calls =
      client_test_interface()->StopRecordingAndGetRecordedFunctionCalls();
  EXPECT_EQ(calls, "RemoveAccount");
  EXPECT_EQ(kNormalizedOtherPrincipal, mgr_->GetActiveAccount());
}

// RemoveAccount fails to remove unknown account.
TEST_F(KerberosCredentialsManagerTest, RemoveAccountFailsUnknownAccount) {
  AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kOneAccount);
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
  client_test_interface()->StartRecordingFunctionCalls();

  RemoveAccount(kOtherPrincipal, kerberos::ERROR_UNKNOWN_PRINCIPAL_NAME,
                kNoNotification, kNoAccount);

  const std::string calls =
      client_test_interface()->StopRecordingAndGetRecordedFunctionCalls();
  EXPECT_EQ(calls, "RemoveAccount");
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
}

// All accounts are wiped when prefs::KerberosEnabled is turned off.
TEST_F(KerberosCredentialsManagerTest, UpdateEnabledFromPrefKerberosDisabled) {
  // Starting with Kerberos enabled.
  SetPref(prefs::kKerberosEnabled, base::Value(true));
  EXPECT_TRUE(mgr_->IsKerberosEnabled());

  AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kOneAccount);
  AddAccountAndAuthenticate(kOtherPrincipal, kManaged, kerberos::ERROR_NONE,
                            kOneNotification, kTwoAccounts);

  SetPref(prefs::kKerberosEnabled, base::Value(false));

  EXPECT_FALSE(mgr_->IsKerberosEnabled());
  EXPECT_EQ(0u, ListAccounts().size());
  EXPECT_TRUE(mgr_->GetActiveAccount().empty());
}

// Managed accounts are restored when prefs::KerberosEnabled is turned on.
TEST_F(KerberosCredentialsManagerTest, UpdateEnabledFromPrefKerberosEnabled) {
  mgr_->SetAddManagedAccountCallbackForTesting(
      GetRepeatingCallback(kTwoAccounts));

  base::Value::Dict managed_account_1;
  base::Value::Dict managed_account_2;

  managed_account_1.Set(kKeyPrincipal, kPrincipal);
  managed_account_2.Set(kKeyPrincipal, kOtherPrincipal);

  base::Value::List managed_accounts;
  managed_accounts.Append(std::move(managed_account_1));
  managed_accounts.Append(std::move(managed_account_2));

  SetPref(prefs::kKerberosAccounts, base::Value(std::move(managed_accounts)));

  EXPECT_FALSE(mgr_->IsKerberosEnabled());
  EXPECT_EQ(0u, ListAccounts().size());
  EXPECT_TRUE(mgr_->GetActiveAccount().empty());

  SetPref(prefs::kKerberosEnabled, base::Value(true));

  // Two notifications are expected: one from AddAccountRunner and another from
  // RemoveAllManagedAccountsExcept.
  WaitAndVerifyResult(GetRepeatedError(kerberos::ERROR_NONE, kTwoAccounts),
                      kTwoNotifications, kTwoAccounts);

  EXPECT_TRUE(mgr_->IsKerberosEnabled());
  EXPECT_EQ(2u, ListAccounts().size());
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
}

// No password is deleted when prefs::kKerberosRememberPasswordEnabled is turned
// on.
TEST_F(KerberosCredentialsManagerTest,
       UpdateRememberPasswordEnabledFromPrefEnabled) {
  client_test_interface()->StartRecordingFunctionCalls();

  SetPref(prefs::kKerberosRememberPasswordEnabled, base::Value(true));

  // Checks that ClearAccounts() was not called on KerberosClient.
  const std::string calls =
      client_test_interface()->StopRecordingAndGetRecordedFunctionCalls();
  EXPECT_TRUE(calls.empty());
}

// All unmanaged passwords are deleted when
// prefs::kKerberosRememberPasswordEnabled is turned off.
TEST_F(KerberosCredentialsManagerTest,
       UpdateRememberPasswordEnabledFromPrefDisabled) {
  mgr_->AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kPassword,
                                  kRememberPassword, kConfig, kAllowExisting,
                                  GetResultCallback());
  mgr_->AddAccountAndAuthenticate(kOtherPrincipal, kManaged, kPassword,
                                  kRememberPassword, kConfig, kAllowExisting,
                                  GetResultCallback());

  WaitAndVerifyResult(GetRepeatedError(kerberos::ERROR_NONE, kTwoAccounts),
                      kOneNotification, kTwoAccounts);

  SetPref(prefs::kKerberosRememberPasswordEnabled, base::Value(false));

  Accounts accounts = ListAccounts();
  ASSERT_EQ(2u, accounts.size());
  EXPECT_FALSE(accounts[0].is_managed());
  EXPECT_FALSE(accounts[0].password_was_remembered());
  EXPECT_TRUE(accounts[1].is_managed());
  EXPECT_TRUE(accounts[1].password_was_remembered());
}

// Setting prefs::kKerberosAddAccountsAllowed to true should not cause any
// account to be deleted.
TEST_F(KerberosCredentialsManagerTest,
       UpdateAddAccountsAllowedFromPrefEnabled) {
  // Starting with KerberosAddAccount disabled.
  SetPref(prefs::kKerberosAddAccountsAllowed, base::Value(false));

  client_test_interface()->StartRecordingFunctionCalls();

  SetPref(prefs::kKerberosAddAccountsAllowed, base::Value(true));

  // Checks that ClearAccounts() was not called on KerberosClient.
  const std::string calls =
      client_test_interface()->StopRecordingAndGetRecordedFunctionCalls();
  EXPECT_TRUE(calls.empty());
}

// All unmanaged accounts are deleted when prefs::kKerberosAddAccountsAllowed is
// turned off.
TEST_F(KerberosCredentialsManagerTest,
       UpdateAddAccountsAllowedFromPrefDisabled) {
  AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kOneAccount);
  AddAccountAndAuthenticate(kOtherPrincipal, kManaged, kerberos::ERROR_NONE,
                            kOneNotification, kTwoAccounts);

  SetPref(prefs::kKerberosAddAccountsAllowed, base::Value(false));

  Accounts accounts = ListAccounts();
  ASSERT_EQ(1u, accounts.size());
  EXPECT_EQ(kNormalizedOtherPrincipal, accounts[0].principal_name());
}

// UpdateAccountsFromPref votes for not saving the password if Kerberos is
// disabled. Also, no account is added.
TEST_F(KerberosCredentialsManagerTest, UpdateAccountsFromPrefKerberosDisabled) {
  base::Value::Dict managed_account;
  managed_account.Set(kKeyPrincipal, kPrincipal);

  base::Value::List managed_accounts;
  managed_accounts.Append(std::move(managed_account));

  SetPref(prefs::kKerberosAccounts, base::Value(std::move(managed_accounts)));

  VerifyVotedForSavingLoginPassword(kDontSaveLoginPassword);

  EXPECT_EQ(0u, ListAccounts().size());
  EXPECT_TRUE(mgr_->GetActiveAccount().empty());
}

// UpdateAccountsFromPref votes for not saving the password if no account is
// saved in prefs. Also, existing managed accounts are deleted.
TEST_F(KerberosCredentialsManagerTest, UpdateAccountsFromPrefNoAccounts) {
  // Starting with Kerberos enabled.
  SetPref(prefs::kKerberosEnabled, base::Value(true));

  AddAccountAndAuthenticate(kPrincipal, kUnmanaged, kerberos::ERROR_NONE,
                            kOneNotification, kOneAccount);
  AddAccountAndAuthenticate(kOtherPrincipal, kManaged, kerberos::ERROR_NONE,
                            kOneNotification, kTwoAccounts);

  base::Value::List managed_accounts;
  SetPref(prefs::kKerberosAccounts, base::Value(std::move(managed_accounts)));

  VerifyVotedForSavingLoginPassword(kDontSaveLoginPassword);

  Accounts accounts = ListAccounts();
  ASSERT_EQ(1u, accounts.size());
  EXPECT_EQ(kNormalizedPrincipal, accounts[0].principal_name());
}

// UpdateAccountsFromPref ignores accounts with bad principal names.
TEST_F(KerberosCredentialsManagerTest, UpdateAccountsFromPrefBadPrincipal) {
  // Starting with Kerberos enabled.
  SetPref(prefs::kKerberosEnabled, base::Value(true));

  base::Value::Dict managed_account_1;
  base::Value::Dict managed_account_2;
  base::Value::Dict managed_account_3;
  base::Value::Dict managed_account_4;
  base::Value::Dict managed_account_5;
  base::Value::Dict managed_account_6;

  managed_account_1.Set(kKeyPrincipal, kBadPrincipal1);
  managed_account_2.Set(kKeyPrincipal, kBadPrincipal2);
  managed_account_3.Set(kKeyPrincipal, kBadPrincipal3);
  managed_account_4.Set(kKeyPrincipal, kBadPrincipal4);
  managed_account_5.Set(kKeyPrincipal, kBadPrincipal5);
  managed_account_6.Set(kKeyPrincipal, kBadManagedPrincipal);

  base::Value::List managed_accounts;
  managed_accounts.Append(std::move(managed_account_1));
  managed_accounts.Append(std::move(managed_account_2));
  managed_accounts.Append(std::move(managed_account_3));
  managed_accounts.Append(std::move(managed_account_4));
  managed_accounts.Append(std::move(managed_account_5));
  managed_accounts.Append(std::move(managed_account_6));

  SetPref(prefs::kKerberosAccounts, base::Value(std::move(managed_accounts)));

  VerifyVotedForSavingLoginPassword(kDontSaveLoginPassword);

  EXPECT_EQ(0u, ListAccounts().size());
  EXPECT_TRUE(mgr_->GetActiveAccount().empty());
}

// UpdateAccountsFromPref uses config if given and default config if not.
TEST_F(KerberosCredentialsManagerTest, UpdateAccountsFromPrefConfig) {
  // Starting with Kerberos enabled.
  SetPref(prefs::kKerberosEnabled, base::Value(true));

  mgr_->SetAddManagedAccountCallbackForTesting(
      GetRepeatingCallback(kTwoAccounts));

  base::Value::List config;
  config.Append(base::Value("config line 1"));
  config.Append(base::Value("config line 2"));
  config.Append(base::Value("config line 3"));

  constexpr char expected_config[] =
      "config line 1\nconfig line 2\nconfig line 3\n";

  base::Value::Dict managed_account_1;
  base::Value::Dict managed_account_2;

  managed_account_1.Set(kKeyPrincipal, kPrincipal);
  managed_account_1.Set(kKeyPassword, kPassword);
  managed_account_1.Set(kKeyKrb5Conf, std::move(config));

  managed_account_2.Set(kKeyPrincipal, kOtherPrincipal);
  managed_account_2.Set(kKeyPassword, kPassword);

  base::Value::List managed_accounts;
  managed_accounts.Append(std::move(managed_account_1));
  managed_accounts.Append(std::move(managed_account_2));

  SetPref(prefs::kKerberosAccounts, base::Value(std::move(managed_accounts)));

  // Two notifications are expected: one from AddAccountRunner and another from
  // RemoveAllManagedAccountsExcept().
  WaitAndVerifyResult(GetRepeatedError(kerberos::ERROR_NONE, kTwoAccounts),
                      kTwoNotifications, kTwoAccounts);

  VerifyVotedForSavingLoginPassword(kDontSaveLoginPassword);

  Accounts accounts = ListAccounts();
  ASSERT_EQ(2u, accounts.size());
  EXPECT_EQ(kNormalizedPrincipal, accounts[0].principal_name());
  EXPECT_EQ(expected_config, accounts[0].krb5conf());
  EXPECT_EQ(kNormalizedOtherPrincipal, accounts[1].principal_name());
  EXPECT_EQ(mgr_->GetDefaultKerberosConfig(), accounts[1].krb5conf());
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
}

// UpdateAccountsFromPref votes for saving login password if any of the
// passwords is equal to "${PASSWORD}".
TEST_F(KerberosCredentialsManagerTest, UpdateAccountsFromPrefPassword) {
  // Starting with Kerberos enabled.
  SetPref(prefs::kKerberosEnabled, base::Value(true));

  mgr_->SetAddManagedAccountCallbackForTesting(
      GetRepeatingCallback(kTwoAccounts));

  base::Value::Dict managed_account_1;
  base::Value::Dict managed_account_2;

  managed_account_1.Set(kKeyPrincipal, kPrincipal);
  managed_account_1.Set(kKeyPassword, kLoginPasswordPlaceholder);
  managed_account_2.Set(kKeyPrincipal, kOtherPrincipal);
  managed_account_2.Set(kKeyPassword, kPassword);

  base::Value::List managed_accounts;
  managed_accounts.Append(std::move(managed_account_1));
  managed_accounts.Append(std::move(managed_account_2));

  SetPref(prefs::kKerberosAccounts, base::Value(std::move(managed_accounts)));

  // Two notifications are expected: one from AddAccountRunner and another from
  // RemoveAllManagedAccountsExcept().
  WaitAndVerifyResult(GetRepeatedError(kerberos::ERROR_NONE, kTwoAccounts),
                      kTwoNotifications, kTwoAccounts);

  VerifyVotedForSavingLoginPassword(kSaveLoginPassword);

  Accounts accounts = ListAccounts();
  ASSERT_EQ(2u, accounts.size());
  EXPECT_EQ(kNormalizedPrincipal, accounts[0].principal_name());
  EXPECT_TRUE(accounts[0].use_login_password());
  EXPECT_EQ(kNormalizedOtherPrincipal, accounts[1].principal_name());
  EXPECT_FALSE(accounts[1].use_login_password());
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
}

// UpdateAccountsFromPref remembers password for accounts with kRememberPassword
// unset or set to true.
TEST_F(KerberosCredentialsManagerTest, UpdateAccountsFromPrefRememberPassword) {
  // Starting with Kerberos enabled.
  SetPref(prefs::kKerberosEnabled, base::Value(true));

  mgr_->SetAddManagedAccountCallbackForTesting(
      GetRepeatingCallback(kThreeAccounts));

  base::Value::Dict managed_account_1;
  base::Value::Dict managed_account_2;
  base::Value::Dict managed_account_3;

  managed_account_1.Set(kKeyPrincipal, kPrincipal);
  managed_account_1.Set(kKeyPassword, kPassword);
  managed_account_2.Set(kKeyPrincipal, kOtherPrincipal);
  managed_account_2.Set(kKeyPassword, kPassword);
  managed_account_2.Set(kKeyRememberPasswordFromPolicy, kRememberPassword);
  managed_account_3.Set(kKeyPrincipal, kYetAnotherPrincipal);
  managed_account_3.Set(kKeyPassword, kPassword);
  managed_account_3.Set(kKeyRememberPasswordFromPolicy, kDontRememberPassword);

  base::Value::List managed_accounts;
  managed_accounts.Append(std::move(managed_account_1));
  managed_accounts.Append(std::move(managed_account_2));
  managed_accounts.Append(std::move(managed_account_3));

  SetPref(prefs::kKerberosAccounts, base::Value(std::move(managed_accounts)));

  // Two notifications are expected: one from AddAccountRunner and another from
  // RemoveAllManagedAccountsExcept().
  WaitAndVerifyResult(GetRepeatedError(kerberos::ERROR_NONE, kThreeAccounts),
                      kTwoNotifications, kThreeAccounts);

  VerifyVotedForSavingLoginPassword(kDontSaveLoginPassword);

  Accounts accounts = ListAccounts();
  ASSERT_EQ(3u, accounts.size());
  EXPECT_TRUE(accounts[0].password_was_remembered());
  EXPECT_TRUE(accounts[1].password_was_remembered());
  EXPECT_FALSE(accounts[2].password_was_remembered());
}

// UpdateAccountsFromPref clears out old managed accounts not in
// prefs::kKerberosAccounts anymore.
TEST_F(KerberosCredentialsManagerTest, UpdateAccountsFromPrefClearAccounts) {
  // Starting with Kerberos enabled.
  SetPref(prefs::kKerberosEnabled, base::Value(true));

  AddAccountAndAuthenticate(kPrincipal, kManaged, kerberos::ERROR_NONE,
                            kOneNotification, kOneAccount);
  AddAccountAndAuthenticate(kYetAnotherPrincipal, kManaged,
                            kerberos::ERROR_NONE, kOneNotification,
                            kTwoAccounts);

  mgr_->SetAddManagedAccountCallbackForTesting(
      GetRepeatingCallback(kTwoAccounts));

  base::Value::Dict managed_account_1;
  base::Value::Dict managed_account_2;

  managed_account_1.Set(kKeyPrincipal, kPrincipal);
  managed_account_2.Set(kKeyPrincipal, kOtherPrincipal);

  base::Value::List managed_accounts;
  managed_accounts.Append(std::move(managed_account_1));
  managed_accounts.Append(std::move(managed_account_2));

  SetPref(prefs::kKerberosAccounts, base::Value(std::move(managed_accounts)));

  // Two notifications are expected: one from AddAccountRunner and another from
  // RemoveAllManagedAccountsExcept().
  WaitAndVerifyResult(GetRepeatedError(kerberos::ERROR_NONE, kTwoAccounts),
                      kTwoNotifications, kTwoAccounts);

  VerifyVotedForSavingLoginPassword(kDontSaveLoginPassword);

  Accounts accounts = ListAccounts();
  ASSERT_EQ(2u, accounts.size());
  EXPECT_EQ(kNormalizedPrincipal, accounts[0].principal_name());
  EXPECT_EQ(kNormalizedOtherPrincipal, accounts[1].principal_name());
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
}

// UpdateAccountsFromPref retries to add account if addition fails for network
// related errors.
// TODO(crbug.com/40715458): Disabled due to flakiness.
TEST_F(KerberosCredentialsManagerTest, DISABLED_UpdateAccountsFromPrefRetry) {
  // Starting with Kerberos enabled.
  SetPref(prefs::kKerberosEnabled, base::Value(true));

  client_test_interface()->SetSimulatedNumberOfNetworkFailures(kOneFailure *
                                                               kOneAccount);

  mgr_->SetAddManagedAccountCallbackForTesting(
      GetRepeatingCallback((kOneFailure + 1) * kOneAccount));

  base::Value::Dict managed_account_1;

  managed_account_1.Set(kKeyPrincipal, kPrincipal);
  managed_account_1.Set(kKeyPassword, kPassword);

  base::Value::List managed_accounts;
  managed_accounts.Append(std::move(managed_account_1));

  SetPref(prefs::kKerberosAccounts, base::Value(std::move(managed_accounts)));

  // Two notifications are expected for each attempt: one from AddAccountRunner
  // and another from RemoveAllManagedAccountsExcept().
  WaitAndVerifyResult({kerberos::ERROR_NETWORK_PROBLEM, kerberos::ERROR_NONE},
                      (kOneFailure + 1) * kTwoNotifications, kOneAccount);

  // Fast forwarding the task environment to force all pending tasks to be
  // executed. Makes sure no retry is scheduled after a successful attempt.
  task_environment_.FastForwardBy(kLongTimeDelay);

  Accounts accounts = ListAccounts();
  ASSERT_EQ(1u, accounts.size());
  EXPECT_EQ(kNormalizedPrincipal, accounts[0].principal_name());
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
}

// UpdateAccountsFromPref retries multiple times to add account if addition
// fails multiple times for network related errors.
// Disable for being flaky. crbug.com/1116500
TEST_F(KerberosCredentialsManagerTest,
       DISABLED_UpdateAccountsFromPrefMultipleRetries) {
  // Starting with Kerberos enabled.
  SetPref(prefs::kKerberosEnabled, base::Value(true));

  client_test_interface()->SetSimulatedNumberOfNetworkFailures(kThreeFailures *
                                                               kOneAccount);

  mgr_->SetAddManagedAccountCallbackForTesting(
      GetRepeatingCallback((kThreeFailures + 1) * kOneAccount));

  base::Value::Dict managed_account_1;

  managed_account_1.Set(kKeyPrincipal, kPrincipal);
  managed_account_1.Set(kKeyPassword, kPassword);

  base::Value::List managed_accounts;
  managed_accounts.Append(std::move(managed_account_1));

  SetPref(prefs::kKerberosAccounts, base::Value(std::move(managed_accounts)));

  // Two notifications are expected for each attempt: one from AddAccountRunner
  // and another from RemoveAllManagedAccountsExcept().
  WaitAndVerifyResult(
      {kerberos::ERROR_NETWORK_PROBLEM, kerberos::ERROR_NETWORK_PROBLEM,
       kerberos::ERROR_NETWORK_PROBLEM, kerberos::ERROR_NONE},
      (kThreeFailures + 1) * kTwoNotifications, kOneAccount);

  // Fast forwarding the task environment to force all pending tasks to be
  // executed. This will make sure no retry is scheduled after a successful
  // attempt.
  task_environment_.FastForwardBy(kLongTimeDelay);

  Accounts accounts = ListAccounts();
  ASSERT_EQ(1u, accounts.size());
  EXPECT_EQ(kNormalizedPrincipal, accounts[0].principal_name());
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
}

// UpdateAccountsFromPref retries to add multiple accounts if addition fails for
// network related errors.
// Disable for being flaky. crbug.com/1116500
TEST_F(KerberosCredentialsManagerTest,
       DISABLED_UpdateAccountsFromPrefRetryMultipleAccounts) {
  // Starting with Kerberos enabled.
  SetPref(prefs::kKerberosEnabled, base::Value(true));

  client_test_interface()->SetSimulatedNumberOfNetworkFailures(kOneFailure *
                                                               kTwoAccounts);

  mgr_->SetAddManagedAccountCallbackForTesting(
      GetRepeatingCallback((kOneFailure + 1) * kTwoAccounts));

  base::Value::Dict managed_account_1;
  base::Value::Dict managed_account_2;

  managed_account_1.Set(kKeyPrincipal, kPrincipal);
  managed_account_1.Set(kKeyPassword, kPassword);
  managed_account_2.Set(kKeyPrincipal, kOtherPrincipal);
  managed_account_2.Set(kKeyPassword, kPassword);

  base::Value::List managed_accounts;
  managed_accounts.Append(std::move(managed_account_1));
  managed_accounts.Append(std::move(managed_account_2));

  SetPref(prefs::kKerberosAccounts, base::Value(std::move(managed_accounts)));

  // Two notifications are expected for each attempt: one from AddAccountRunner
  // and another from RemoveAllManagedAccountsExcept().
  WaitAndVerifyResult(
      {kerberos::ERROR_NETWORK_PROBLEM, kerberos::ERROR_NETWORK_PROBLEM,
       kerberos::ERROR_NONE, kerberos::ERROR_NONE},
      (kOneFailure + 1) * kTwoNotifications, kTwoAccounts);

  // Fast forwarding the task environment to force all pending tasks to be
  // executed. This will make sure no retry is scheduled after a successful
  // attempt.
  task_environment_.FastForwardBy(kLongTimeDelay);

  Accounts accounts = ListAccounts();
  ASSERT_EQ(2u, accounts.size());
  EXPECT_EQ(kNormalizedPrincipal, accounts[0].principal_name());
  EXPECT_EQ(kNormalizedOtherPrincipal, accounts[1].principal_name());
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
}

// UpdateAccountsFromPref stops retrying after a certain number of network
// related errors.
// Disable for being flaky. crbug.com/1116500
TEST_F(KerberosCredentialsManagerTest,
       DISABLED_UpdateAccountsFromPrefStopsRetrying) {
  // Starting with Kerberos enabled.
  SetPref(prefs::kKerberosEnabled, base::Value(true));

  client_test_interface()->SetSimulatedNumberOfNetworkFailures(kLotsOfFailures);

  mgr_->SetAddManagedAccountCallbackForTesting(GetRepeatingCallback(
      KerberosCredentialsManager::kMaxFailureCountForManagedAccounts *
      kTwoAccounts));

  base::Value::Dict managed_account_1;
  base::Value::Dict managed_account_2;

  managed_account_1.Set(kKeyPrincipal, kPrincipal);
  managed_account_1.Set(kKeyPassword, kPassword);
  managed_account_2.Set(kKeyPrincipal, kOtherPrincipal);
  managed_account_2.Set(kKeyPassword, kPassword);

  base::Value::List managed_accounts;
  managed_accounts.Append(std::move(managed_account_1));
  managed_accounts.Append(std::move(managed_account_2));

  SetPref(prefs::kKerberosAccounts, base::Value(std::move(managed_accounts)));

  // Two notifications are expected for each attempt: one from AddAccountRunner
  // and another from RemoveAllManagedAccountsExcept().
  WaitAndVerifyResult(
      GetRepeatedError(
          kerberos::ERROR_NETWORK_PROBLEM,
          KerberosCredentialsManager::kMaxFailureCountForManagedAccounts *
              kTwoAccounts),
      KerberosCredentialsManager::kMaxFailureCountForManagedAccounts *
          kTwoNotifications,
      kTwoAccounts);

  // Fast forwarding the task environment to force all pending tasks to be
  // executed. This will make sure no retry is scheduled after a certain number
  // of network related errors.
  task_environment_.FastForwardBy(kLongTimeDelay);

  Accounts accounts = ListAccounts();
  ASSERT_EQ(2u, accounts.size());
  EXPECT_EQ(kNormalizedPrincipal, accounts[0].principal_name());
  EXPECT_EQ(kNormalizedOtherPrincipal, accounts[1].principal_name());
  EXPECT_EQ(kNormalizedPrincipal, mgr_->GetActiveAccount());
}

// TODO(https://crbug.com/952251): Add more tests
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
// - Notification
//     + If the notification is clicked, shows the KerberosAccounts settings
//       with ?kerberos_reauth=<principal name>
// - If policy service finishes initialization after constructor of
//   KerberosCredentialsManager, UpdateAccountsFromPref is called.
//
// See also
//   https://analysis.chromium.org/p/chromium/coverage/dir?host=chromium.googlesource.com&project=chromium/src&ref=refs/heads/main&path=//chrome/browser/ash/kerberos/&platform=linux-chromeos
// for code coverage (try to get as high as possible!).

}  // namespace ash
