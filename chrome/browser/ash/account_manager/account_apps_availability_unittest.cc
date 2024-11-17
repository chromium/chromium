// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_apps_availability.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Contains;
using testing::InSequence;

using Checkpoint = ::testing::MockFunction<void(int step)>;

namespace ash {

namespace {

constexpr char kPrimaryAccountEmail[] = "primaryaccount@gmail.com";
constexpr char kSecondaryAccount1Email[] = "secondaryAccount1@gmail.com";
constexpr char kSecondaryAccount2Email[] = "secondaryAccount2@gmail.com";

account_manager::Account CreateAccount(const std::string& email,
                                       const std::string& gaia_id) {
  account_manager::AccountKey key(gaia_id,
                                  ::account_manager::AccountType::kGaia);
  return {key, email};
}

class MockObserver : public AccountAppsAvailability::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnAccountAvailableInArc,
              (const account_manager::Account&),
              (override));
  MOCK_METHOD(void,
              OnAccountUnavailableInArc,
              (const account_manager::Account&),
              (override));
};

base::flat_set<account_manager::Account> GetAccountsAvailableInArcSync(
    AccountAppsAvailability* availability) {
  base::test::TestFuture<const base::flat_set<account_manager::Account>&>
      future;
  availability->GetAccountsAvailableInArc(future.GetCallback());
  return future.Get();
}

MATCHER_P(AccountEqual, other, "") {
  return arg.key == other.key && arg.raw_email == other.raw_email;
}

}  // namespace

class AccountAppsAvailabilityTest : public testing::Test {
 public:
  AccountAppsAvailabilityTest(const AccountAppsAvailabilityTest&) = delete;
  AccountAppsAvailabilityTest& operator=(const AccountAppsAvailabilityTest&) =
      delete;

 protected:
  AccountAppsAvailabilityTest() = default;
  ~AccountAppsAvailabilityTest() override = default;

  void SetUp() override {
    identity_test_env()->EnableRemovalOfExtendedAccountInfo();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    AccountAppsAvailability::RegisterPrefs(pref_service_->registry());

    fake_user_manager_.Reset(std::make_unique<user_manager::FakeUserManager>());

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
    auto* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(), false,
                                     false);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  AccountInfo primary_account_;
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
};

TEST_F(AccountAppsAvailabilityTest, InitializationPrefIsPersistedOnDisk) {
  base::HistogramTester tester;
  auto account_apps_availability = CreateAccountAppsAvailability();
  EXPECT_FALSE(account_apps_availability->IsInitialized());
  // Wait for `GetAccounts` call to finish.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(account_apps_availability->IsInitialized());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      0u,
      tester.GetAllSamples(AccountAppsAvailability::kNumAccountsInArcMetricName)
          .size());
  EXPECT_EQ(0u,
            tester
                .GetAllSamples(
                    AccountAppsAvailability::kPercentAccountsInArcMetricName)
                .size());
  account_apps_availability.reset();

  account_apps_availability = CreateAccountAppsAvailability();
  EXPECT_TRUE(account_apps_availability->IsInitialized());
  // Wait for `GetAccounts` call to finish.
  base::RunLoop().RunUntilIdle();
  tester.ExpectUniqueSample(
      AccountAppsAvailability::kNumAccountsInArcMetricName,
      /*sample=*/1, /*expected_bucket_count=*/1);
  tester.ExpectUniqueSample(
      AccountAppsAvailability::kPercentAccountsInArcMetricName,
      /*sample=*/100, /*expected_bucket_count=*/1);
}

TEST_F(AccountAppsAvailabilityTest, CallsBeforeInitialization) {
  const AccountInfo secondary_account_info =
      identity_test_env()->MakeAccountAvailable(kSecondaryAccount1Email);
  const account_manager::Account primary_account =
      CreateAccount(kPrimaryAccountEmail, primary_account_info()->gaia);
  const account_manager::Account secondary_account =
      CreateAccount(kSecondaryAccount1Email, secondary_account_info.gaia);

  auto account_apps_availability = CreateAccountAppsAvailability();
  EXPECT_FALSE(account_apps_availability->IsInitialized());
  // Make secondary account unavailable in ARC.
  account_apps_availability->SetIsAccountAvailableInArc(secondary_account,
                                                        false);

  base::test::TestFuture<const base::flat_set<account_manager::Account>&>
      future;
  // Wait for initialization and for `GetAccountsAvailableInArc` call
  // completion.
  account_apps_availability->GetAccountsAvailableInArc(future.GetCallback());
  base::flat_set<account_manager::Account> result = future.Get();
  EXPECT_TRUE(account_apps_availability->IsInitialized());

  // Only primary account is available, secondary account was removed.
  EXPECT_EQ(result.size(), 1u);
  EXPECT_THAT(result, Contains(AccountEqual(primary_account)));
}

TEST_F(AccountAppsAvailabilityTest, GetAccountsAvailableInArc) {
  const AccountInfo secondary_account_info =
      identity_test_env()->MakeAccountAvailable(kSecondaryAccount1Email);
  const account_manager::Account primary_account =
      CreateAccount(kPrimaryAccountEmail, primary_account_info()->gaia);
  const account_manager::Account secondary_account =
      CreateAccount(kSecondaryAccount1Email, secondary_account_info.gaia);

  auto account_apps_availability = CreateAccountAppsAvailability();
  // Wait for initialization to finish.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(account_apps_availability->IsInitialized());
  // All accounts are available after initialization:
  auto accounts =
      GetAccountsAvailableInArcSync(account_apps_availability.get());
  EXPECT_EQ(accounts.size(), 2u);
  EXPECT_THAT(accounts, Contains(AccountEqual(primary_account)));
  EXPECT_THAT(accounts, Contains(AccountEqual(secondary_account)));

  // Remove an account from ARC.
  account_apps_availability->SetIsAccountAvailableInArc(secondary_account,
                                                        false);
  auto accounts_1 =
      GetAccountsAvailableInArcSync(account_apps_availability.get());
  EXPECT_EQ(accounts_1.size(), 1u);
  EXPECT_THAT(accounts_1, Contains(AccountEqual(primary_account)));
}

TEST_F(AccountAppsAvailabilityTest, SetIsAccountAvailableInArc) {
  const AccountInfo secondary_account_1_info =
      identity_test_env()->MakeAccountAvailable(kSecondaryAccount1Email);
  const account_manager::Account primary_account =
      CreateAccount(kPrimaryAccountEmail, primary_account_info()->gaia);
  const account_manager::Account secondary_account_1 =
      CreateAccount(kSecondaryAccount1Email, secondary_account_1_info.gaia);

  auto account_apps_availability = CreateAccountAppsAvailability();
  // Wait for initialization to finish.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(account_apps_availability->IsInitialized());
  // All accounts are available after initialization:
  {
    auto accounts =
        GetAccountsAvailableInArcSync(account_apps_availability.get());
    EXPECT_EQ(accounts.size(), 2u);
    EXPECT_THAT(accounts, Contains(AccountEqual(primary_account)));
    EXPECT_THAT(accounts, Contains(AccountEqual(secondary_account_1)));
  }

  // Remove an account from ARC.
  account_apps_availability->SetIsAccountAvailableInArc(secondary_account_1,
                                                        false);
  {
    auto accounts =
        GetAccountsAvailableInArcSync(account_apps_availability.get());
    EXPECT_EQ(accounts.size(), 1u);
    EXPECT_THAT(accounts, Contains(AccountEqual(primary_account)));
  }

  const AccountInfo secondary_account_2_info =
      identity_test_env()->MakeAccountAvailable(kSecondaryAccount2Email);
  const account_manager::Account secondary_account_2 =
      CreateAccount(kSecondaryAccount2Email, secondary_account_2_info.gaia);
  // Add the account to ARC.
  account_apps_availability->SetIsAccountAvailableInArc(secondary_account_2,
                                                        true);
  {
    auto accounts =
        GetAccountsAvailableInArcSync(account_apps_availability.get());
    EXPECT_EQ(accounts.size(), 2u);
    EXPECT_THAT(accounts, Contains(AccountEqual(primary_account)));
    EXPECT_THAT(accounts, Contains(AccountEqual(secondary_account_2)));
  }

  // Remove the first account again.
  account_apps_availability->SetIsAccountAvailableInArc(secondary_account_1,
                                                        false);
  // Add the second account again.
  account_apps_availability->SetIsAccountAvailableInArc(secondary_account_2,
                                                        true);
  {
    auto accounts =
        GetAccountsAvailableInArcSync(account_apps_availability.get());
    EXPECT_EQ(accounts.size(), 2u);
    EXPECT_THAT(accounts, Contains(AccountEqual(primary_account)));
    EXPECT_THAT(accounts, Contains(AccountEqual(secondary_account_2)));
  }

  // Add the first account back.
  account_apps_availability->SetIsAccountAvailableInArc(secondary_account_1,
                                                        true);
  // Remove the second account.
  account_apps_availability->SetIsAccountAvailableInArc(secondary_account_2,
                                                        false);
  {
    auto accounts =
        GetAccountsAvailableInArcSync(account_apps_availability.get());
    EXPECT_EQ(accounts.size(), 2u);
    EXPECT_THAT(accounts, Contains(AccountEqual(primary_account)));
    EXPECT_THAT(accounts, Contains(AccountEqual(secondary_account_1)));
  }
}

TEST_F(AccountAppsAvailabilityTest, ObserversAreCalledWhenAvailabilityChanges) {
  const AccountInfo secondary_account_1_info =
      identity_test_env()->MakeAccountAvailable(kSecondaryAccount1Email);
  const account_manager::Account primary_account =
      CreateAccount(kPrimaryAccountEmail, primary_account_info()->gaia);
  const account_manager::Account secondary_account_1 =
      CreateAccount(kSecondaryAccount1Email, secondary_account_1_info.gaia);

  auto account_apps_availability = CreateAccountAppsAvailability();
  // Wait for initialization to finish.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(account_apps_availability->IsInitialized());

  MockObserver mock_observer;
  base::ScopedObservation<AccountAppsAvailability,
                          AccountAppsAvailability::Observer>
      observation{&mock_observer};
  observation.Observe(account_apps_availability.get());

  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(mock_observer,
                OnAccountUnavailableInArc(AccountEqual(secondary_account_1)))
        .Times(1);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_observer,
                OnAccountAvailableInArc(AccountEqual(secondary_account_1)))
        .Times(1);
  }

  // [Account is available in ARC] Remove an account from ARC - observer is
  // called.
  account_apps_availability->SetIsAccountAvailableInArc(secondary_account_1,
                                                        false);

  checkpoint.Call(1);

  // [Account is NOT available in ARC] Add an account to ARC - observer is
  // called.
  account_apps_availability->SetIsAccountAvailableInArc(secondary_account_1,
                                                        true);
}

TEST_F(AccountAppsAvailabilityTest,
       ObserversAreNotCalledWhenAvailabilityDoesntChange) {
  const AccountInfo secondary_account_1_info =
      identity_test_env()->MakeAccountAvailable(kSecondaryAccount1Email);
  const account_manager::Account primary_account =
      CreateAccount(kPrimaryAccountEmail, primary_account_info()->gaia);
  const account_manager::Account secondary_account_1 =
      CreateAccount(kSecondaryAccount1Email, secondary_account_1_info.gaia);

  auto account_apps_availability = CreateAccountAppsAvailability();
  // Wait for initialization to finish.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(account_apps_availability->IsInitialized());

  MockObserver mock_observer;
  base::ScopedObservation<AccountAppsAvailability,
                          AccountAppsAvailability::Observer>
      observation{&mock_observer};
  observation.Observe(account_apps_availability.get());

  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(mock_observer, OnAccountAvailableInArc(_)).Times(0);
    EXPECT_CALL(mock_observer, OnAccountUnavailableInArc(_)).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_observer, OnAccountAvailableInArc(_)).Times(0);
    EXPECT_CALL(mock_observer, OnAccountUnavailableInArc(_)).Times(0);
  }

  // [Account is available in ARC] Add the same account again - observer is not
  // called.
  account_apps_availability->SetIsAccountAvailableInArc(secondary_account_1,
                                                        true);
  checkpoint.Call(1);

  const AccountInfo secondary_account_2_info =
      identity_test_env()->MakeAccountAvailable(kSecondaryAccount2Email);
  const account_manager::Account secondary_account_2 =
      CreateAccount(kSecondaryAccount2Email, secondary_account_2_info.gaia);

  // [Account is NOT available in ARC] Account is removed from ARC - observer is
  // not called.
  account_apps_availability->SetIsAccountAvailableInArc(secondary_account_2,
                                                        false);
}

TEST_F(AccountAppsAvailabilityTest,
       ObserversAreCalledWhenAvailableAccountIsChanged) {
  const AccountInfo secondary_account_1_info =
      identity_test_env()->MakeAccountAvailable(kSecondaryAccount1Email);
  const account_manager::Account primary_account =
      CreateAccount(kPrimaryAccountEmail, primary_account_info()->gaia);
  const account_manager::Account secondary_account_1 =
      CreateAccount(kSecondaryAccount1Email, secondary_account_1_info.gaia);

  auto account_apps_availability = CreateAccountAppsAvailability();
  // Wait for initialization to finish.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(account_apps_availability->IsInitialized());

  MockObserver mock_observer;
  base::ScopedObservation<AccountAppsAvailability,
                          AccountAppsAvailability::Observer>
      observation{&mock_observer};
  observation.Observe(account_apps_availability.get());

  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(mock_observer,
                OnAccountAvailableInArc(AccountEqual(secondary_account_1)))
        .Times(1);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_observer,
                OnAccountUnavailableInArc(AccountEqual(secondary_account_1)))
        .Times(1);
  }

  // [Account is available in ARC] Account is upserted - observer is called.
  identity_test_env()->SetRefreshTokenForAccount(
      secondary_account_1_info.account_id);
  // Wait for async calls to finish.
  base::RunLoop().RunUntilIdle();
  checkpoint.Call(1);

  // [Account is available in ARC] Account is removed - observer is
  // called.
  identity_test_env()->RemoveRefreshTokenForAccount(
      secondary_account_1_info.account_id);
  // Wait for async calls to finish.
  base::RunLoop().RunUntilIdle();
}

TEST_F(AccountAppsAvailabilityTest,
       ObserversAreNotCalledWhenUnavailableAccountIsChanged) {
  const AccountInfo secondary_account_1_info =
      identity_test_env()->MakeAccountAvailable(kSecondaryAccount1Email);
  const account_manager::Account primary_account =
      CreateAccount(kPrimaryAccountEmail, primary_account_info()->gaia);
  const account_manager::Account secondary_account_1 =
      CreateAccount(kSecondaryAccount1Email, secondary_account_1_info.gaia);

  auto account_apps_availability = CreateAccountAppsAvailability();
  // Wait for initialization to finish.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(account_apps_availability->IsInitialized());

  MockObserver mock_observer;
  base::ScopedObservation<AccountAppsAvailability,
                          AccountAppsAvailability::Observer>
      observation{&mock_observer};
  observation.Observe(account_apps_availability.get());

  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(mock_observer,
                OnAccountUnavailableInArc(AccountEqual(secondary_account_1)))
        .Times(1);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_observer, OnAccountAvailableInArc(_)).Times(0);
    EXPECT_CALL(mock_observer, OnAccountUnavailableInArc(_)).Times(0);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_observer, OnAccountAvailableInArc(_)).Times(0);
    EXPECT_CALL(mock_observer, OnAccountUnavailableInArc(_)).Times(0);
  }

  // Remove an account from ARC.
  account_apps_availability->SetIsAccountAvailableInArc(secondary_account_1,
                                                        false);
  checkpoint.Call(1);

  // [Account is NOT available in ARC] Account is upserted - observer is not
  // called.
  identity_test_env()->SetRefreshTokenForAccount(
      secondary_account_1_info.account_id);
  // Wait for async calls to finish.
  base::RunLoop().RunUntilIdle();
  checkpoint.Call(2);

  // [Account is NOT available in ARC] Account is removed - observer is not
  // called.
  identity_test_env()->RemoveRefreshTokenForAccount(
      secondary_account_1_info.account_id);
  // Wait for async calls to finish.
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
