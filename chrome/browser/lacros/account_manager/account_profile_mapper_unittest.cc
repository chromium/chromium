// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"

#include <algorithm>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_result.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using account_manager::Account;
using account_manager::AccountAdditionResult;
using account_manager::AccountKey;
using account_manager::AccountManagerFacade;
using testing::Field;

namespace {

constexpr account_manager::AccountType kGaiaType =
    account_manager::AccountType::kGaia;

// Map from profile path to a vector of GaiaIds.
using AccountMapping =
    base::flat_map<base::FilePath, base::flat_set<std::string>>;

class MockAccountProfileMapperObserver : public AccountProfileMapper::Observer {
 public:
  MockAccountProfileMapperObserver() = default;
  ~MockAccountProfileMapperObserver() override = default;

  MOCK_METHOD(void,
              OnAccountUpserted,
              (const base::FilePath& profile_path, const Account&),
              (override));
  MOCK_METHOD(void,
              OnAccountRemoved,
              (const base::FilePath& profile_path, const Account&),
              (override));
};

MATCHER_P(OptionalAccountEqual, other, "optional<Account> equality matcher") {
  if (arg == absl::nullopt && other == absl::nullopt)
    return true;
  return arg->key == other->key && arg->raw_email == other->raw_email;
}

// Synthetizes a non-Gaia `Account` from an id.
Account NonGaiaAccountFromID(const std::string& id) {
  AccountKey key(id, account_manager::AccountType::kActiveDirectory);
  return {key, id + std::string("@example.com")};
}

// Synthetizes a `Account` from a Gaia ID, with a dummy email.
Account AccountFromGaiaID(const std::string& gaia_id) {
  AccountKey key(gaia_id, kGaiaType);
  return {key, gaia_id + std::string("@gmail.com")};
}

// Similar to `AccountFromGaiaID()`, but operates on vectors.
std::vector<Account> AccountsFromGaiaIDs(
    const std::vector<std::string>& gaia_ids) {
  std::vector<Account> accounts;
  for (const auto& id : gaia_ids)
    accounts.push_back(AccountFromGaiaID(id));
  return accounts;
}

}  // namespace

class AccountProfileMapperTest : public testing::Test {
 public:
  AccountProfileMapperTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    CHECK(testing_profile_manager_.SetUp(base::FilePath(".")));
    ON_CALL(mock_facade_, GetPersistentErrorForAccount)
        .WillByDefault(
            [](const AccountKey&,
               base::OnceCallback<void(const GoogleServiceAuthError&)>
                   callback) {
              std::move(callback).Run(GoogleServiceAuthError::AuthErrorNone());
            });
  }

  ProfileAttributesStorage* attributes_storage() {
    return &testing_profile_manager_.profile_manager()
                ->GetProfileAttributesStorage();
  }

  account_manager::MockAccountManagerFacade* mock_facade() {
    return &mock_facade_;
  }

  const base::FilePath& main_path() { return main_path_; }

  // Helper function, similar to TestMapperUpdate(), but assumes all accounts
  // are Gaia.
  void TestMapperUpdateGaia(
      AccountProfileMapper* mapper,
      const std::vector<std::string>& gaia_accounts_in_facade,
      const AccountMapping& expected_accounts_upserted,
      const AccountMapping& expected_accounts_removed,
      const AccountMapping& expected_accounts_in_storage) {
    TestMapperUpdate(mapper, AccountsFromGaiaIDs(gaia_accounts_in_facade),
                     expected_accounts_upserted, expected_accounts_removed,
                     expected_accounts_in_storage);
  }

  // Triggers an update of the accounts and checks observer calls, and the end
  // state of the storage.
  void TestMapperUpdate(AccountProfileMapper* mapper,
                        const std::vector<Account>& accounts_in_facade,
                        const AccountMapping& expected_accounts_upserted,
                        const AccountMapping& expected_accounts_removed,
                        const AccountMapping& expected_accounts_in_storage) {
    MockAccountProfileMapperObserver mock_observer;
    base::ScopedObservation<AccountProfileMapper,
                            AccountProfileMapper::Observer>
        observation{&mock_observer};
    observation.Observe(mapper);
    ExpectOnAccountUpserted(&mock_observer, expected_accounts_upserted);
    ExpectOnAccountRemoved(&mock_observer, expected_accounts_removed);

    // Trigger a `GetAccounts()` call.
    ExpectFacadeGetAccountsCalled();
    mapper->OnAccountUpserted(AccountFromGaiaID("Dummy"));
    CompleteFacadeGetAccounts(accounts_in_facade);

    testing::Mock::VerifyAndClearExpectations(&mock_observer);
    testing::Mock::VerifyAndClearExpectations(mock_facade());
    EXPECT_TRUE(CheckAccountsInStorage(expected_accounts_in_storage));
  }

  std::unique_ptr<AccountProfileMapper> GetMapperNonInitialized(
      const AccountMapping& accounts) {
    SetAccountsInStorage(accounts);
    EXPECT_TRUE(CheckAccountsInStorage(accounts));
    ExpectFacadeGetAccountsCalled();
    std::unique_ptr<AccountProfileMapper> mapper =
        std::make_unique<AccountProfileMapper>(mock_facade(),
                                               attributes_storage());
    return mapper;
  }

  std::unique_ptr<AccountProfileMapper> GetMapper(
      const AccountMapping& accounts) {
    std::unique_ptr<AccountProfileMapper> mapper =
        GetMapperNonInitialized(accounts);
    // Initialize the mapper by completing the `GetAccounts()` call on the
    // facade.
    std::vector<std::string> accounts_in_facade;
    for (const auto& path_accounts_pair : accounts) {
      for (const std::string& id : path_accounts_pair.second) {
        accounts_in_facade.push_back(id);
      }
    }
    CompleteFacadeGetAccountsGaia(accounts_in_facade);
    return mapper;
  }

  // Setup gMock expectations for `OnAccountUpserted()` calls.
  void ExpectOnAccountUpserted(MockAccountProfileMapperObserver* mock_observer,
                               const AccountMapping& accounts_map) {
    if (accounts_map.empty()) {
      EXPECT_CALL(*mock_observer, OnAccountUpserted(testing::_, testing::_))
          .Times(0);
      return;
    }
    for (const auto& path_accounts_pair : accounts_map) {
      const base::FilePath profile_path = path_accounts_pair.first;
      for (const std::string& gaia_id : path_accounts_pair.second) {
        AccountKey key = {gaia_id, kGaiaType};
        EXPECT_CALL(*mock_observer,
                    OnAccountUpserted(profile_path, Field(&Account::key, key)));
      }
    }
  }

  // Setup gMock expectations for `OnAccountRemoved()` calls.
  void ExpectOnAccountRemoved(MockAccountProfileMapperObserver* mock_observer,
                              const AccountMapping& accounts_map) {
    if (accounts_map.empty()) {
      EXPECT_CALL(*mock_observer, OnAccountRemoved(testing::_, testing::_))
          .Times(0);
      return;
    }
    for (const auto& path_accounts_pair : accounts_map) {
      const base::FilePath profile_path = path_accounts_pair.first;
      for (const std::string& gaia_id : path_accounts_pair.second) {
        AccountKey key = {gaia_id, kGaiaType};
        EXPECT_CALL(*mock_observer,
                    OnAccountRemoved(profile_path, Field(&Account::key, key)));
      }
    }
  }

  // Checks that the `ProfileAttributesStorage` matches `accounts_map`.
  bool CheckAccountsInStorage(const AccountMapping& accounts_map) {
    auto entries = attributes_storage()->GetAllProfilesAttributes();
    if (entries.size() != accounts_map.size())
      return false;
    bool main_profile_found = false;
    for (const ProfileAttributesEntry* entry : entries) {
      const base::FilePath path = entry->GetPath();
      if (Profile::IsMainProfilePath(path)) {
        if (main_profile_found)
          return false;  // Duplicate main profile.
        main_profile_found = true;
      }
      if (!accounts_map.contains(path))
        return false;  // Profile not found.
      if (entry->GetGaiaIds() != accounts_map.at(path))
        return false;  // Accounts don't match.
    }
    if (!main_profile_found)
      return false;  // No main profile.
    return true;
  }

  // Sets an expectation that `GetAccounts()` is called on the facade, and
  // stores the callback for later use in `CompleteFacadeGetAccounts()`.
  void ExpectFacadeGetAccountsCalled() {
    EXPECT_CALL(mock_facade_, GetAccounts(testing::_))
        .WillOnce([this](base::OnceCallback<void(const std::vector<Account>&)>
                             callback) {
          DCHECK(!facade_get_accounts_completion_);
          facade_get_accounts_completion_ = std::move(callback);
        });
  }

  // Sets an expectation that `ShowAddAccountDialog()` is called on the facade,
  // and immediately returns with a new account.
  void ExpectFacadeShowAddAccountDialogCalled(
      AccountManagerFacade::AccountAdditionSource source,
      const absl::optional<Account>& new_account) {
    EXPECT_CALL(mock_facade_, ShowAddAccountDialog(source, testing::_))
        .WillOnce(
            [new_account](AccountManagerFacade::AccountAdditionSource,
                          base::OnceCallback<void(const AccountAdditionResult&)>
                              callback) {
              std::move(callback).Run(
                  new_account.has_value()
                      ? AccountAdditionResult::FromAccount(new_account.value())
                      : AccountAdditionResult::FromStatus(
                            AccountAdditionResult::Status::kCancelledByUser));
            });
  }

  void CompleteFacadeGetAccountsGaia(const std::vector<std::string>& gaia_ids) {
    CompleteFacadeGetAccounts(AccountsFromGaiaIDs(gaia_ids));
  }

  void CompleteFacadeGetAccounts(const std::vector<Account>& accounts) {
    std::move(facade_get_accounts_completion_).Run(accounts);
  }

  // Sets the accounts in `ProfileAttributesStorage`. `accounts_map` is a map
  // from profile path to a vector of GaiaIds. One of the profiles must be the
  // main profile.
  void SetAccountsInStorage(const AccountMapping& accounts_map) {
    ProfileAttributesStorage* storage = attributes_storage();
    // Clear the storage.
    std::vector<base::FilePath> profile_paths;
    for (const auto* entry : storage->GetAllProfilesAttributes())
      profile_paths.push_back(entry->GetPath());
    for (const base::FilePath& path : profile_paths)
      storage->RemoveProfile(path);
    // Import accounts from the map.
    for (const auto& path_accounts_pair : accounts_map) {
      const base::FilePath path = path_accounts_pair.first;
      ProfileAttributesInitParams init_params;
      init_params.profile_path = path;
      storage->AddProfile(std::move(init_params));
      storage->GetProfileAttributesWithPath(path)->SetGaiaIds(
          path_accounts_pair.second);
    }
  }

 private:
  const base::FilePath main_path_ = base::FilePath("Default");
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  account_manager::MockAccountManagerFacade mock_facade_;
  base::OnceCallback<void(const std::vector<Account>&)>
      facade_get_accounts_completion_;
};

// Test basic functionality for `GetAccounts()`:
// - returns expected accounts when called on a valid profile
// - returns no accounts when called on non-existing profile
// - does not trigger a call to GetAccounts() on the facade.
TEST_F(AccountProfileMapperTest, GetAccounts) {
  base::FilePath other_path("Other");
  std::unique_ptr<AccountProfileMapper> mapper =
      GetMapper({{main_path(), {"A"}}, {other_path, {"B", "C"}}});

  base::MockRepeatingCallback<void(const std::vector<Account>&)> mock_callback;

  // `GetAccounts()` does not go through the facade, but directly reads from
  // storage.
  EXPECT_CALL(*mock_facade(), GetAccounts(testing::_)).Times(0);

  // Non-existing profile.
  EXPECT_CALL(mock_callback, Run(testing::IsEmpty()));
  mapper->GetAccounts(base::FilePath("MissingAccount"), mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Existing profile.
  EXPECT_CALL(mock_callback,
              Run(testing::UnorderedElementsAre(
                  Field(&Account::key, AccountKey{"B", kGaiaType}),
                  Field(&Account::key, AccountKey{"C", kGaiaType}))));
  mapper->GetAccounts(other_path, mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // No call to the facade.
  testing::Mock::VerifyAndClearExpectations(mock_facade());
}

// Tests that accounts are added by default to the main profile when there is
// only one profile.
TEST_F(AccountProfileMapperTest, UpdateSingleProfile) {
  std::unique_ptr<AccountProfileMapper> mapper =
      GetMapper({{main_path(), {"A", "B"}}});
  TestMapperUpdateGaia(mapper.get(),
                       /*accounts_in_facade=*/{"A", "C"},
                       /*expected_accounts_upserted=*/{{main_path(), {"C"}}},
                       /*expected_accounts_removed=*/{{main_path(), {"B"}}},
                       /*expected_accounts_in_storage=*/
                       {{main_path(), {"A", "C"}}});
}

// Tests that new accounts are left unassigned when there are multiple profiles.
TEST_F(AccountProfileMapperTest, UpdateMulltiProfile) {
  base::FilePath other_path("Other");
  std::unique_ptr<AccountProfileMapper> mapper =
      GetMapper({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  TestMapperUpdateGaia(
      mapper.get(),
      /*accounts_in_facade=*/{"A", "B", "D"},
      /*expected_accounts_upserted=*/{{base::FilePath(), {"D"}}},
      /*expected_accounts_removed=*/{{other_path, {"C"}}},
      /*expected_accounts_in_storage=*/
      {{main_path(), {"A"}}, {other_path, {"B"}}});
}

// Checks that `GetPersistentErrorForAccount()` returns an error when the
// account is not in this profile.
TEST_F(AccountProfileMapperTest, GetPersistentErrorForAccount) {
  base::FilePath other_path("Other");
  std::unique_ptr<AccountProfileMapper> mapper =
      GetMapper({{main_path(), {"A"}}, {other_path, {"B"}}});
  base::MockRepeatingCallback<void(const GoogleServiceAuthError&)>
      mock_callback;

  // Account exists in the profile: success.
  EXPECT_CALL(mock_callback, Run(GoogleServiceAuthError::AuthErrorNone()));
  mapper->GetPersistentErrorForAccount(main_path(), {"A", kGaiaType},
                                       mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Account does not exist in the profile: failure.
  EXPECT_CALL(
      mock_callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP)));
  mapper->GetPersistentErrorForAccount(main_path(), {"B", kGaiaType},
                                       mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);
}

// Tests that consumer callbacks are delayed until initialization completes.
TEST_F(AccountProfileMapperTest, WaitForInitialization) {
  std::unique_ptr<AccountProfileMapper> mapper =
      GetMapperNonInitialized({{main_path(), {"A", "B"}}});
  base::MockOnceCallback<void(const GoogleServiceAuthError&)> error_callback;
  base::MockOnceCallback<void(const std::vector<Account>&)> accounts_callback;
  // Call the mapper before initialization, callback not invoked.
  EXPECT_CALL(error_callback, Run(testing::_)).Times(0);
  EXPECT_CALL(accounts_callback, Run(testing::_)).Times(0);
  mapper->GetPersistentErrorForAccount(main_path(), {"A", kGaiaType},
                                       error_callback.Get());
  mapper->GetAccounts(main_path(), accounts_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&error_callback);
  testing::Mock::VerifyAndClearExpectations(&accounts_callback);
  // Complete initialization: callback is invoked.
  EXPECT_CALL(error_callback, Run(GoogleServiceAuthError::AuthErrorNone()));
  EXPECT_CALL(accounts_callback,
              Run(testing::UnorderedElementsAre(
                  Field(&Account::key, AccountKey{"A", kGaiaType}),
                  Field(&Account::key, AccountKey{"B", kGaiaType}))));
  CompleteFacadeGetAccountsGaia({"A", "B"});
  testing::Mock::VerifyAndClearExpectations(&error_callback);
  testing::Mock::VerifyAndClearExpectations(&accounts_callback);
}

TEST_F(AccountProfileMapperTest, NoObserversAtInitialization) {
  std::unique_ptr<AccountProfileMapper> mapper =
      GetMapperNonInitialized({{main_path(), {"A"}}});
  // Change the storage, so that observers would normally trigger.
  SetAccountsInStorage({{main_path(), {"A", "B"}}});

  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper.get());
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);

  // Observers were not called even though the storage was updated.
  EXPECT_TRUE(CheckAccountsInStorage({{main_path(), {"A", "B"}}}));
  CompleteFacadeGetAccountsGaia({"A"});
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  EXPECT_TRUE(CheckAccountsInStorage({{main_path(), {"A"}}}));
}

TEST_F(AccountProfileMapperTest, NonGaia) {
  std::unique_ptr<AccountProfileMapper> mapper =
      GetMapper({{main_path(), {"A"}}});
  // Addition of non-Gaia account is ignored.
  TestMapperUpdate(mapper.get(),
                   {AccountFromGaiaID("A"), NonGaiaAccountFromID("B")},
                   /*expected_accounts_upserted=*/{},
                   /*expected_accounts_removed=*/{},
                   /*expected_accounts_in_storage=*/
                   {{main_path(), {"A"}}});
  // Removal is ignored as well.
  TestMapperUpdate(mapper.get(), {AccountFromGaiaID("A")},
                   /*expected_accounts_upserted=*/{},
                   /*expected_accounts_removed=*/{},
                   /*expected_accounts_in_storage=*/
                   {{main_path(), {"A"}}});
}

TEST_F(AccountProfileMapperTest, ShowAddAccountDialogBeforeInit) {
  std::unique_ptr<AccountProfileMapper> mapper =
      GetMapperNonInitialized({{main_path(), {"A"}}});
  AccountManagerFacade::AccountAdditionSource source =
      AccountManagerFacade::AccountAdditionSource::kArc;
  // The facade is not called before initialization.
  EXPECT_CALL(*mock_facade(), ShowAddAccountDialog(testing::_, testing::_))
      .Times(0);
  mapper->ShowAddAccountDialog(
      main_path(), source,
      base::OnceCallback<void(const absl::optional<Account>&)>());
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  // Complete initialization, and check that the facade was called.
  EXPECT_CALL(*mock_facade(), ShowAddAccountDialog(source, testing::_));
  CompleteFacadeGetAccountsGaia({"A"});
  testing::Mock::VerifyAndClearExpectations(mock_facade());
}

TEST_F(AccountProfileMapperTest, ShowAddAccountDialog) {
  base::FilePath other_path("Other");
  std::unique_ptr<AccountProfileMapper> mapper =
      GetMapper({{main_path(), {"A"}}, {other_path, {"B"}}});
  base::MockOnceCallback<void(const absl::optional<Account>&)>
      account_added_callback;
  AccountManagerFacade::AccountAdditionSource source =
      AccountManagerFacade::AccountAdditionSource::kArc;
  Account account_c = AccountFromGaiaID("C");
  // Add account to existing profile.
  ExpectFacadeShowAddAccountDialogCalled(source, account_c);
  EXPECT_CALL(account_added_callback,
              Run(OptionalAccountEqual(absl::make_optional(account_c))));
  mapper->ShowAddAccountDialog(other_path, source,
                               account_added_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  // Add account that already exists.
  ExpectFacadeShowAddAccountDialogCalled(source, account_c);
  EXPECT_CALL(account_added_callback,
              Run(OptionalAccountEqual(absl::optional<Account>())));
  mapper->ShowAddAccountDialog(other_path, source,
                               account_added_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  // Add account to non-existing profile.
  ExpectFacadeShowAddAccountDialogCalled(source, AccountFromGaiaID("D"));
  EXPECT_CALL(account_added_callback,
              Run(OptionalAccountEqual(absl::optional<Account>())));
  mapper->ShowAddAccountDialog(base::FilePath("UnknownProfile"), source,
                               account_added_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  // Non-Gaia account.
  ExpectFacadeShowAddAccountDialogCalled(source, NonGaiaAccountFromID("D"));
  EXPECT_CALL(account_added_callback,
              Run(OptionalAccountEqual(absl::optional<Account>())));
  mapper->ShowAddAccountDialog(other_path, source,
                               account_added_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  // Flow aborted.
  ExpectFacadeShowAddAccountDialogCalled(source, absl::nullopt);
  EXPECT_CALL(account_added_callback,
              Run(OptionalAccountEqual(absl::optional<Account>())));
  mapper->ShowAddAccountDialog(other_path, source,
                               account_added_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
}
