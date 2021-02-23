// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/account_manager/account_manager_ash.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include "ash/components/account_manager/account_manager.h"
#include "ash/components/account_manager/account_manager_ui.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/account_manager.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

namespace {

const char kFakeGaiaId[] = "fake-gaia-id";
const char kFakeEmail[] = "fake_email@example.com";
const char kFakeToken[] = "fake-token";
const account_manager::Account kFakeAccount = account_manager::Account{
    account_manager::AccountKey{kFakeGaiaId,
                                account_manager::AccountType::kGaia},
    kFakeEmail};

}  // namespace

class TestAccountManagerObserver
    : public mojom::AccountManagerObserverInterceptorForTesting {
 public:
  TestAccountManagerObserver() : receiver_(this) {}

  TestAccountManagerObserver(const TestAccountManagerObserver&) = delete;
  TestAccountManagerObserver& operator=(const TestAccountManagerObserver&) =
      delete;
  ~TestAccountManagerObserver() override = default;

  void Observe(
      mojom::AccountManagerAsyncWaiter* const account_manager_async_waiter) {
    mojo::PendingReceiver<mojom::AccountManagerObserver> receiver;
    account_manager_async_waiter->AddObserver(&receiver);
    receiver_.Bind(std::move(receiver));
  }

  int GetNumOnTokenUpsertedCalls() { return num_token_upserted_calls_; }

  account_manager::Account GetLastUpsertedAccount() {
    return last_upserted_account_;
  }

  int GetNumOnAccountRemovedCalls() { return num_account_removed_calls_; }

  account_manager::Account GetLastRemovedAccount() {
    return last_removed_account_;
  }

 private:
  // mojom::AccountManagerObserverInterceptorForTesting override:
  AccountManagerObserver* GetForwardingInterface() override { return this; }

  // mojom::AccountManagerObserverInterceptorForTesting override:
  void OnTokenUpserted(mojom::AccountPtr account) override {
    ++num_token_upserted_calls_;
    last_upserted_account_ = account_manager::FromMojoAccount(account).value();
  }

  // mojom::AccountManagerObserverInterceptorForTesting override:
  void OnAccountRemoved(mojom::AccountPtr account) override {
    ++num_account_removed_calls_;
    last_removed_account_ = account_manager::FromMojoAccount(account).value();
  }

  int num_token_upserted_calls_ = 0;
  int num_account_removed_calls_ = 0;
  account_manager::Account last_upserted_account_;
  account_manager::Account last_removed_account_;
  mojo::Receiver<mojom::AccountManagerObserver> receiver_;
};

class FakeAccountManagerUI : public ash::AccountManagerUI {
 public:
  FakeAccountManagerUI() {}
  FakeAccountManagerUI(const FakeAccountManagerUI&) = delete;
  FakeAccountManagerUI& operator=(const FakeAccountManagerUI&) = delete;
  ~FakeAccountManagerUI() override {}

  void SetIsDialogShown(bool is_dialog_shown) {
    is_dialog_shown_ = is_dialog_shown;
  }

  void CloseDialog() {
    if (!close_dialog_closure_.is_null()) {
      std::move(close_dialog_closure_).Run();
    }
    is_dialog_shown_ = false;
  }

  int show_account_addition_dialog_calls() const {
    return show_account_addition_dialog_calls_;
  }

  int show_account_reauthentication_dialog_calls() const {
    return show_account_reauthentication_dialog_calls_;
  }

  int show_manage_accounts_settings_calls() const {
    return show_manage_accounts_settings_calls_;
  }

 private:
  // AccountManagerUI overrides:
  void ShowAddAccountDialog(base::OnceClosure close_dialog_closure) override {
    close_dialog_closure_ = std::move(close_dialog_closure);
    show_account_addition_dialog_calls_++;
    is_dialog_shown_ = true;
  }
  void ShowReauthAccountDialog(
      const std::string& email,
      base::OnceClosure close_dialog_closure) override {
    close_dialog_closure_ = std::move(close_dialog_closure);
    show_account_reauthentication_dialog_calls_++;
    is_dialog_shown_ = true;
  }
  bool IsDialogShown() override { return is_dialog_shown_; }
  void ShowManageAccountsSettings() override {
    show_manage_accounts_settings_calls_++;
  }

  base::OnceClosure close_dialog_closure_;
  bool is_dialog_shown_ = false;
  int show_account_addition_dialog_calls_ = 0;
  int show_account_reauthentication_dialog_calls_ = 0;
  int show_manage_accounts_settings_calls_ = 0;
};

class AccountManagerAshTest : public ::testing::Test {
 public:
  AccountManagerAshTest() = default;
  AccountManagerAshTest(const AccountManagerAshTest&) = delete;
  AccountManagerAshTest& operator=(const AccountManagerAshTest&) = delete;
  ~AccountManagerAshTest() override = default;

 protected:
  void SetUp() override {
    account_manager_ash_ =
        std::make_unique<AccountManagerAsh>(&account_manager_);
    account_manager_ash_->SetAccountManagerUI(
        std::make_unique<FakeAccountManagerUI>());
    account_manager_ash_->BindReceiver(remote_.BindNewPipeAndPassReceiver());
    account_manager_async_waiter_ =
        std::make_unique<mojom::AccountManagerAsyncWaiter>(
            account_manager_ash_.get());
  }

  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

  void FlushMojoForTesting() { account_manager_ash_->FlushMojoForTesting(); }

  // Returns |true| if initialization was successful.
  bool InitializeAccountManager() {
    base::RunLoop run_loop;
    account_manager_.InitializeInEphemeralMode(
        test_url_loader_factory_.GetSafeWeakWrapper(), run_loop.QuitClosure());
    account_manager_.SetPrefService(&pref_service_);
    account_manager_.RegisterPrefs(pref_service_.registry());
    run_loop.Run();
    return account_manager_.IsInitialized();
  }

  FakeAccountManagerUI* GetFakeAccountManagerUI() {
    return static_cast<FakeAccountManagerUI*>(
        account_manager_ash_->account_manager_ui_.get());
  }

  mojom::AccountAdditionResultPtr ShowAddAccountDialog(
      base::OnceClosure quit_closure) {
    auto add_account_result = mojom::AccountAdditionResult::New();
    account_manager_ash_->ShowAddAccountDialog(base::BindOnce(
        [](base::OnceClosure quit_closure,
           mojom::AccountAdditionResultPtr* add_account_result,
           mojom::AccountAdditionResultPtr result) {
          (*add_account_result)->status = result->status;
          (*add_account_result)->account = std::move(result->account);
          std::move(quit_closure).Run();
        },
        std::move(quit_closure), &add_account_result));
    return add_account_result;
  }

  void ShowReauthAccountDialog(const std::string& email,
                               base::OnceClosure close_dialog_closure) {
    account_manager_ash_->ShowReauthAccountDialog(
        email, std::move(close_dialog_closure));
  }

  void CallAccountAdditionFinished(
      const account_manager::AccountAdditionResult& result) {
    account_manager_ash_->OnAccountAdditionFinished(result);
    GetFakeAccountManagerUI()->CloseDialog();
  }

  void ShowManageAccountsSettings() {
    account_manager_ash_->ShowManageAccountsSettings();
  }

  int GetNumObservers() { return account_manager_ash_->observers_.size(); }

  mojom::AccountManagerAsyncWaiter* account_manager_async_waiter() {
    return account_manager_async_waiter_.get();
  }

  ash::AccountManager* account_manager() { return &account_manager_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple pref_service_;
  ash::AccountManager account_manager_;
  mojo::Remote<mojom::AccountManager> remote_;
  std::unique_ptr<AccountManagerAsh> account_manager_ash_;
  std::unique_ptr<mojom::AccountManagerAsyncWaiter>
      account_manager_async_waiter_;
};

TEST_F(AccountManagerAshTest,
       IsInitializedReturnsFalseForUninitializedAccountManager) {
  bool is_initialized = true;
  account_manager_async_waiter()->IsInitialized(&is_initialized);
  EXPECT_FALSE(is_initialized);
}

TEST_F(AccountManagerAshTest,
       IsInitializedReturnsTrueForInitializedAccountManager) {
  bool is_initialized = true;
  account_manager_async_waiter()->IsInitialized(&is_initialized);
  EXPECT_FALSE(is_initialized);
  ASSERT_TRUE(InitializeAccountManager());
  account_manager_async_waiter()->IsInitialized(&is_initialized);
  EXPECT_TRUE(is_initialized);
}

// Test that lacros remotes do not leak.
TEST_F(AccountManagerAshTest,
       LacrosRemotesAreAutomaticallyRemovedOnConnectionClose) {
  EXPECT_EQ(0, GetNumObservers());
  {
    mojo::PendingReceiver<mojom::AccountManagerObserver> receiver;
    account_manager_async_waiter()->AddObserver(&receiver);
    EXPECT_EQ(1, GetNumObservers());
  }
  // Wait for the disconnect handler to be called.
  RunAllPendingTasks();
  EXPECT_EQ(0, GetNumObservers());
}

TEST_F(AccountManagerAshTest, LacrosObserversAreNotifiedOnAccountUpdates) {
  const account_manager::AccountKey kTestAccountKey{
      kFakeGaiaId, account_manager::AccountType::kGaia};
  ASSERT_TRUE(InitializeAccountManager());
  TestAccountManagerObserver observer;
  observer.Observe(account_manager_async_waiter());
  ASSERT_EQ(1, GetNumObservers());

  EXPECT_EQ(0, observer.GetNumOnTokenUpsertedCalls());
  account_manager()->UpsertAccount(kTestAccountKey, kFakeEmail, kFakeToken);
  FlushMojoForTesting();
  EXPECT_EQ(1, observer.GetNumOnTokenUpsertedCalls());
  EXPECT_EQ(kTestAccountKey, observer.GetLastUpsertedAccount().key);
  EXPECT_EQ(kFakeEmail, observer.GetLastUpsertedAccount().raw_email);
}

TEST_F(AccountManagerAshTest, LacrosObserversAreNotifiedOnAccountRemovals) {
  const account_manager::AccountKey kTestAccountKey{
      kFakeGaiaId, account_manager::AccountType::kGaia};
  ASSERT_TRUE(InitializeAccountManager());
  TestAccountManagerObserver observer;
  observer.Observe(account_manager_async_waiter());
  ASSERT_EQ(1, GetNumObservers());
  account_manager()->UpsertAccount(kTestAccountKey, kFakeEmail, kFakeToken);
  FlushMojoForTesting();

  EXPECT_EQ(0, observer.GetNumOnAccountRemovedCalls());
  account_manager()->RemoveAccount(kTestAccountKey);
  FlushMojoForTesting();
  EXPECT_EQ(1, observer.GetNumOnAccountRemovedCalls());
  EXPECT_EQ(kTestAccountKey, observer.GetLastRemovedAccount().key);
  EXPECT_EQ(kFakeEmail, observer.GetLastRemovedAccount().raw_email);
}

TEST_F(AccountManagerAshTest,
       ShowAddAccountDialogReturnsInProgressIfDialogIsOpen) {
  EXPECT_EQ(0, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(true);
  mojom::AccountAdditionResultPtr account_addition_result;
  account_manager_async_waiter()->ShowAddAccountDialog(
      &account_addition_result);

  // Check status.
  EXPECT_EQ(mojom::AccountAdditionResult::Status::kAlreadyInProgress,
            account_addition_result->status);
  // Check that dialog was not called.
  EXPECT_EQ(0, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
}

TEST_F(AccountManagerAshTest,
       ShowAddAccountDialogReturnsCancelledAfterDialogIsClosed) {
  EXPECT_EQ(0, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(false);

  base::RunLoop run_loop;
  mojom::AccountAdditionResultPtr account_addition_result =
      ShowAddAccountDialog(run_loop.QuitClosure());
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  run_loop.Run();

  // Check status.
  EXPECT_EQ(mojom::AccountAdditionResult::Status::kCancelledByUser,
            account_addition_result->status);
  // Check that dialog was called once.
  EXPECT_EQ(1, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
}

TEST_F(AccountManagerAshTest,
       ShowAddAccountDialogReturnsSuccessAfterAccountIsAdded) {
  EXPECT_EQ(0, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(false);

  base::RunLoop run_loop;
  mojom::AccountAdditionResultPtr account_addition_result =
      ShowAddAccountDialog(run_loop.QuitClosure());
  // Simulate account addition.
  CallAccountAdditionFinished(account_manager::AccountAdditionResult(
      account_manager::AccountAdditionResult::Status::kSuccess, kFakeAccount));
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  run_loop.Run();

  // Check status.
  EXPECT_EQ(mojom::AccountAdditionResult::Status::kSuccess,
            account_addition_result->status);
  // Check account.
  base::Optional<account_manager::Account> account =
      account_manager::FromMojoAccount(account_addition_result->account);
  EXPECT_TRUE(account.has_value());
  EXPECT_EQ(kFakeAccount.key, account.value().key);
  EXPECT_EQ(kFakeAccount.raw_email, account.value().raw_email);
  // Check that dialog was called once.
  EXPECT_EQ(1, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
}

TEST_F(AccountManagerAshTest, ShowAddAccountDialogCanHandleMultipleCalls) {
  EXPECT_EQ(0, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(false);

  base::RunLoop run_loop;
  mojom::AccountAdditionResultPtr account_addition_result =
      ShowAddAccountDialog(run_loop.QuitClosure());

  base::RunLoop run_loop_2;
  mojom::AccountAdditionResultPtr account_addition_result_2 =
      ShowAddAccountDialog(run_loop_2.QuitClosure());
  run_loop_2.Run();
  // The second call gets 'kAlreadyInProgress' reply.
  EXPECT_EQ(mojom::AccountAdditionResult::Status::kAlreadyInProgress,
            account_addition_result_2->status);

  // Simulate account addition.
  CallAccountAdditionFinished(account_manager::AccountAdditionResult(
      account_manager::AccountAdditionResult::Status::kSuccess, kFakeAccount));
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  run_loop.Run();

  EXPECT_EQ(mojom::AccountAdditionResult::Status::kSuccess,
            account_addition_result->status);
  // Check account.
  base::Optional<account_manager::Account> account =
      account_manager::FromMojoAccount(account_addition_result->account);
  EXPECT_TRUE(account.has_value());
  EXPECT_EQ(kFakeAccount.key, account.value().key);
  EXPECT_EQ(kFakeAccount.raw_email, account.value().raw_email);
  // Check that dialog was called once.
  EXPECT_EQ(1, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
}

TEST_F(AccountManagerAshTest,
       ShowAddAccountDialogCanHandleMultipleSequentialCalls) {
  EXPECT_EQ(0, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(false);

  base::RunLoop run_loop;
  mojom::AccountAdditionResultPtr account_addition_result =
      ShowAddAccountDialog(run_loop.QuitClosure());
  // Simulate account addition.
  CallAccountAdditionFinished(account_manager::AccountAdditionResult(
      account_manager::AccountAdditionResult::Status::kSuccess, kFakeAccount));
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  run_loop.Run();
  EXPECT_EQ(mojom::AccountAdditionResult::Status::kSuccess,
            account_addition_result->status);
  // Check account.
  base::Optional<account_manager::Account> account =
      account_manager::FromMojoAccount(account_addition_result->account);
  EXPECT_TRUE(account.has_value());
  EXPECT_EQ(kFakeAccount.key, account.value().key);
  EXPECT_EQ(kFakeAccount.raw_email, account.value().raw_email);

  base::RunLoop run_loop_2;
  mojom::AccountAdditionResultPtr account_addition_result_2 =
      ShowAddAccountDialog(run_loop_2.QuitClosure());
  // Simulate account addition.
  CallAccountAdditionFinished(account_manager::AccountAdditionResult(
      account_manager::AccountAdditionResult::Status::kSuccess, kFakeAccount));
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();
  run_loop_2.Run();
  EXPECT_EQ(mojom::AccountAdditionResult::Status::kSuccess,
            account_addition_result_2->status);
  // Check account.
  base::Optional<account_manager::Account> account_2 =
      account_manager::FromMojoAccount(account_addition_result_2->account);
  EXPECT_TRUE(account_2.has_value());
  EXPECT_EQ(kFakeAccount.key, account_2.value().key);
  EXPECT_EQ(kFakeAccount.raw_email, account_2.value().raw_email);

  // Check that dialog was called 2 times.
  EXPECT_EQ(2, GetFakeAccountManagerUI()->show_account_addition_dialog_calls());
}

TEST_F(AccountManagerAshTest,
       ShowReauthAccountDialogDoesntCallTheDialogIfItsAlreadyShown) {
  EXPECT_EQ(
      0,
      GetFakeAccountManagerUI()->show_account_reauthentication_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(true);
  base::RunLoop run_loop;
  // Simulate account reauthentication.
  ShowReauthAccountDialog(kFakeEmail, run_loop.QuitClosure());
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();

  // Check that dialog was not called.
  EXPECT_EQ(
      0,
      GetFakeAccountManagerUI()->show_account_reauthentication_dialog_calls());
}

TEST_F(AccountManagerAshTest, ShowReauthAccountDialogOpensTheDialog) {
  EXPECT_EQ(
      0,
      GetFakeAccountManagerUI()->show_account_reauthentication_dialog_calls());
  GetFakeAccountManagerUI()->SetIsDialogShown(false);
  base::RunLoop run_loop;
  // Simulate account reauthentication.
  ShowReauthAccountDialog(kFakeEmail, run_loop.QuitClosure());
  // Simulate closing the dialog.
  GetFakeAccountManagerUI()->CloseDialog();

  // Check that dialog was called once.
  EXPECT_EQ(
      1,
      GetFakeAccountManagerUI()->show_account_reauthentication_dialog_calls());
}

TEST_F(AccountManagerAshTest, ShowManageAccountSettingsTest) {
  EXPECT_EQ(0,
            GetFakeAccountManagerUI()->show_manage_accounts_settings_calls());
  ShowManageAccountsSettings();
  EXPECT_EQ(1,
            GetFakeAccountManagerUI()->show_manage_accounts_settings_calls());
}

}  // namespace crosapi
