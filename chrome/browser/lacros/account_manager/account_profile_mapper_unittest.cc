// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/profile_waiter.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using account_manager::Account;
using account_manager::AccountKey;
using account_manager::AccountManagerFacade;
using account_manager::AccountUpsertionResult;
using testing::Field;

namespace {

const char kLacrosAccountIdsPref[] =
    "profile.account_manager_lacros_account_ids";

constexpr account_manager::AccountType kGaiaType =
    account_manager::AccountType::kGaia;

// Map from profile path to a set of GaiaIds.
using AccountMapping =
    base::flat_map<base::FilePath, base::flat_set<std::string>>;

// Map from profile path to a vector of account error updates.
using AccountErrorMapping =
    base::flat_map<base::FilePath,
                   std::vector<std::pair<std::string, GoogleServiceAuthError>>>;

using MockAddAccountCallback = base::MockOnceCallback<void(
    const std::optional<AccountProfileMapper::AddAccountResult>&)>;

class MockAccountProfileMapperObserver : public AccountProfileMapper::Observer {
 public:
  MockAccountProfileMapperObserver() = default;
  ~MockAccountProfileMapperObserver() override = default;

  MOCK_METHOD(void,
              OnAccountUpserted,
              (const base::FilePath&, const Account&),
              (override));
  MOCK_METHOD(void,
              OnAccountRemoved,
              (const base::FilePath&, const Account&),
              (override));
  MOCK_METHOD(void,
              OnAuthErrorChanged,
              (const base::FilePath&,
               const account_manager::AccountKey&,
               const GoogleServiceAuthError&),
              (override));
};

class ProfileAttributesStorageTestObserver
    : public ProfileAttributesStorage::Observer {
 public:
  explicit ProfileAttributesStorageTestObserver(
      ProfileAttributesStorage* storage)
      : storage_(storage) {}

  void WaitForProfileBeingDeleted(const base::FilePath& profile_path) {
    ProfileAttributesEntry* entry =
        storage_->GetProfileAttributesWithPath(profile_path);
    // Return immediately if the profile entry doesn't exist.
    if (!entry)
      return;

    storage_observation_.Observe(storage_.get());
    profile_path_ = profile_path;
    run_loop_.Run();
  }

  void OnProfileWasRemoved(const base::FilePath& removed_profile_path,
                           const std::u16string& profile_name) override {
    if (removed_profile_path != profile_path_)
      return;

    storage_observation_.Reset();
    run_loop_.Quit();
  }

 private:
  raw_ptr<ProfileAttributesStorage> storage_;
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      storage_observation_{this};
  base::FilePath profile_path_;
  base::RunLoop run_loop_;
};

MATCHER_P(AddAccountResultEqual,
          other,
          "optional<AddAccountResult> equality matcher") {
  if (arg == std::nullopt && other == std::nullopt) {
    return true;
  }
  return arg->profile_path == other->profile_path &&
         arg->account.key == other->account.key &&
         arg->account.raw_email == other->account.raw_email;
}

MATCHER_P(AccountEqual, other, "Account equality matcher") {
  return arg.key == other.key && arg.raw_email == other.raw_email;
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
    CHECK(testing_profile_manager_.SetUp());
    main_path_ = GetProfilePath("Default");
    ON_CALL(mock_facade_, GetPersistentErrorForAccount)
        .WillByDefault(
            [](const AccountKey&,
               base::OnceCallback<void(const GoogleServiceAuthError&)>
                   callback) {
              std::move(callback).Run(GoogleServiceAuthError::AuthErrorNone());
            });
  }

  ~AccountProfileMapperTest() override {
    EXPECT_TRUE(facade_get_accounts_completion_callbacks_.empty())
        << "The test has " << facade_get_accounts_completion_callbacks_.size()
        << " unsatisfied GetAccounts() callbacks";
  }

  TestingProfileManager* testing_profile_manager() {
    return &testing_profile_manager_;
  }

  ProfileAttributesStorage* attributes_storage() {
    return &testing_profile_manager_.profile_manager()
                ->GetProfileAttributesStorage();
  }

  account_manager::MockAccountManagerFacade* mock_facade() {
    return &mock_facade_;
  }

  TestingPrefServiceSimple* local_state() {
    return testing_profile_manager_.local_state()->Get();
  }

  const base::FilePath& main_path() { return main_path_; }

  base::FilePath GetProfilePath(const std::string& name) {
    return testing_profile_manager_.profiles_dir().AppendASCII(name);
  }

  // Helper function, similar to TestMapperUpdate(), but assumes all accounts
  // are Gaia.
  void TestMapperUpdateGaia(
      AccountProfileMapper* mapper,
      const std::vector<std::string>& gaia_accounts_in_facade,
      const AccountMapping& expected_accounts_upserted,
      const AccountMapping& expected_accounts_removed,
      const AccountMapping& expected_accounts_in_prefs) {
    TestMapperUpdate(mapper, AccountsFromGaiaIDs(gaia_accounts_in_facade),
                     expected_accounts_upserted, expected_accounts_removed,
                     expected_accounts_in_prefs);
  }

  // Triggers an update of the accounts and checks observer calls, and the end
  // state of the prefs.
  void TestMapperUpdate(AccountProfileMapper* mapper,
                        const std::vector<Account>& accounts_in_facade,
                        const AccountMapping& expected_accounts_upserted,
                        const AccountMapping& expected_accounts_removed,
                        const AccountMapping& expected_accounts_in_prefs) {
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
    VerifyAccountsInPrefs(expected_accounts_in_prefs);
  }

  AccountProfileMapper* CreateMapperNonInitialized(
      const AccountMapping& accounts) {
    ExpectFacadeGetAccountsCalled();
    testing_profile_manager_.SetAccountProfileMapper(
        std::make_unique<AccountProfileMapper>(
            mock_facade(), attributes_storage(), local_state()));
    CreateProfilesAndSetAccountsInPrefs(accounts);
    VerifyAccountsInPrefs(accounts);
    return testing_profile_manager_.profile_manager()
        ->GetAccountProfileMapper();
  }

  AccountProfileMapper* CreateMapper(const AccountMapping& accounts) {
    AccountProfileMapper* mapper = CreateMapperNonInitialized(accounts);
    // Initialize the mapper by completing the `GetAccounts()` call on the
    // facade.
    std::vector<std::string> accounts_in_facade;
    for (const auto& path_accounts_pair : accounts) {
      for (const std::string& id : path_accounts_pair.second) {
        accounts_in_facade.push_back(id);
      }
    }
    CompleteFacadeGetAccountsGaia(accounts_in_facade);
    testing::Mock::VerifyAndClearExpectations(mock_facade());
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

  // Setup gMock expectations for `OnAuthErrorChanged()` calls.
  void ExpectOnAuthErrorChanged(MockAccountProfileMapperObserver* mock_observer,
                                const AccountErrorMapping& account_errors_map) {
    if (account_errors_map.empty()) {
      EXPECT_CALL(*mock_observer,
                  OnAuthErrorChanged(testing::_, testing::_, testing::_))
          .Times(0);
      return;
    }
    for (const auto& path_account_errors : account_errors_map) {
      const base::FilePath profile_path = path_account_errors.first;

      for (const std::pair<std::string, GoogleServiceAuthError>& account_error :
           path_account_errors.second) {
        const AccountKey account_key{account_error.first, kGaiaType};
        const GoogleServiceAuthError error = account_error.second;
        EXPECT_CALL(*mock_observer,
                    OnAuthErrorChanged(profile_path, account_key, error));
      }
    }
  }

  // Checks that the `ProfileAttributesStorage` matches `accounts_map`.
  // Tests should normally use `VerifyAccountsInPrefs()` instead to verify local
  // state as well.
  void VerifyAccountsInStorage(const AccountMapping& accounts_map) {
    auto entries = attributes_storage()->GetAllProfilesAttributes();
    // Count profiles in the map.
    size_t profiles_in_map = accounts_map.size();
    if (accounts_map.contains(base::FilePath()))
      --profiles_in_map;  // Unassigned accounts.
    EXPECT_EQ(entries.size(), profiles_in_map);
    bool main_profile_found = false;
    for (const ProfileAttributesEntry* entry : entries) {
      const base::FilePath path = entry->GetPath();
      if (Profile::IsMainProfilePath(path)) {
        EXPECT_FALSE(main_profile_found) << "Duplicate main profile: " << path;
        main_profile_found = true;
      }
      if (accounts_map.contains(path)) {
        EXPECT_EQ(entry->GetGaiaIds(), accounts_map.at(path))
            << "Accounts don't match";
      } else {
        ADD_FAILURE() << "Profile \"" << path << "\" not found";
      }
    }
    EXPECT_TRUE(main_profile_found) << "No main profile";
  }

  // Checks that the `ProfileAttributesStorage` and the list of accounts in
  // local state match `accounts_map`.
  void VerifyAccountsInPrefs(const AccountMapping& accounts_map) {
    VerifyAccountsInStorage(accounts_map);

    // Check accounts in local state.
    base::flat_set<std::string> accounts_set;
    for (const auto& path_and_accounts_pair : accounts_map) {
      const auto& profile_accounts_set = path_and_accounts_pair.second;
      accounts_set.insert(profile_accounts_set.begin(),
                          profile_accounts_set.end());
    }
    EXPECT_EQ(GetLacrosAccountsFromLocalState(), accounts_set);
  }

  // Sets an expectation that `GetAccounts()` is called on the facade at least
  // once, and stores the callbacks for later use in
  // `CompleteFacadeGetAccounts()`.
  // `CompleteFacadeGetAccounts()` must be called the exact same number of times
  // as `GetAccounts()`.
  void ExpectFacadeGetAccountsCalled() {
    EXPECT_CALL(mock_facade_, GetAccounts(testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly([this](base::OnceCallback<void(
                                   const std::vector<Account>&)> callback) {
          facade_get_accounts_completion_callbacks_.push(std::move(callback));
        });
  }

  // Sets an expectation that `ShowAddAccountDialog()` is called on the facade,
  // and immediately returns with a new account.
  void ExpectFacadeShowAddAccountDialogCalled(
      AccountManagerFacade::AccountAdditionSource source,
      const std::optional<Account>& new_account) {
    EXPECT_CALL(mock_facade_, ShowAddAccountDialog(source, testing::_))
        .WillOnce([new_account](
                      AccountManagerFacade::AccountAdditionSource,
                      base::OnceCallback<void(const AccountUpsertionResult&)>
                          callback) {
          std::move(callback).Run(
              new_account.has_value()
                  ? AccountUpsertionResult::FromAccount(new_account.value())
                  : AccountUpsertionResult::FromStatus(
                        AccountUpsertionResult::Status::kCancelledByUser));
        });
  }

  void CompleteFacadeGetAccountsGaia(const std::vector<std::string>& gaia_ids) {
    CompleteFacadeGetAccounts(AccountsFromGaiaIDs(gaia_ids));
  }

  void CompleteFacadeGetAccounts(const std::vector<Account>& accounts) {
    ASSERT_FALSE(facade_get_accounts_completion_callbacks_.empty());
    auto callback =
        std::move(facade_get_accounts_completion_callbacks_.front());
    facade_get_accounts_completion_callbacks_.pop();
    std::move(callback).Run(accounts);
  }

  // Creates profiles that are listed in `accounts_map` and sets the accounts in
  // `ProfileAttributesStorage` and in local state.
  // `accounts_map` is a map from profile path to a vector of GaiaIds. One of
  // the profiles must be the main profile.
  void CreateProfilesAndSetAccountsInPrefs(const AccountMapping& accounts_map) {
    // Clear all profiles.
    testing_profile_manager_.DeleteAllTestingProfiles();
    // Create new profiles.
    for (const auto& path_accounts_pair : accounts_map) {
      const base::FilePath path = path_accounts_pair.first;
      if (path.empty())
        continue;  // Account is unassigned.
      testing_profile_manager_.CreateTestingProfile(
          path.BaseName().MaybeAsASCII());
    }
    SetAccountsInStorage(accounts_map);
    base::flat_set<std::string> accounts_set;
    for (const auto& path_and_accounts_pair : accounts_map) {
      const auto& profile_accounts_set = path_and_accounts_pair.second;
      accounts_set.insert(profile_accounts_set.begin(),
                          profile_accounts_set.end());
    }
    SetLacrosAccountsInLocalState(accounts_set);
  }

  // Imports accounts from `accounts_map` to `ProfileAttributesStorage`.
  void SetAccountsInStorage(const AccountMapping& accounts_map) {
    ProfileAttributesStorage* storage = attributes_storage();
    for (const auto& path_accounts_pair : accounts_map) {
      const base::FilePath& path = path_accounts_pair.first;
      if (path.empty())
        continue;  // Account is unassigned.
      storage->GetProfileAttributesWithPath(path)->SetGaiaIds(
          path_accounts_pair.second);
    }
  }

  void SetLacrosAccountsInLocalState(
      const base::flat_set<std::string>& account_ids) {
    base::Value::List list;
    for (const auto& gaia_id : account_ids)
      list.Append(gaia_id);
    local_state()->SetList(kLacrosAccountIdsPref, std::move(list));
  }

  base::flat_set<std::string> GetLacrosAccountsFromLocalState() {
    const base::Value& list = local_state()->GetValue(kLacrosAccountIdsPref);
    EXPECT_TRUE(list.is_list());
    return base::MakeFlatSet<std::string>(
        list.GetList(), {},
        [](const base::Value& value) { return value.GetString(); });
  }

  void SetPrimaryAccountForProfile(const base::FilePath& profile_path,
                                   const std::string& primary_gaia_id,
                                   bool is_consented_primary_account = true,
                                   bool is_managed = false) {
    ProfileAttributesStorage* storage = attributes_storage();
    ProfileAttributesEntry* entry =
        storage->GetProfileAttributesWithPath(profile_path);
    ASSERT_TRUE(entry);
    entry->SetAuthInfo(primary_gaia_id, u"Test", is_consented_primary_account);
    if (is_managed)
      entry->SetHostedDomain("managed.com");
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  account_manager::MockAccountManagerFacade mock_facade_;
  std::queue<base::OnceCallback<void(const std::vector<Account>&)>>
      facade_get_accounts_completion_callbacks_;
  TestingProfileManager testing_profile_manager_;
  base::FilePath main_path_;
};

// Test basic functionality for `GetAccounts()`:
// - returns expected accounts when called on a valid profile
// - returns no accounts when called on non-existing profile
// - does not trigger a call to GetAccounts() on the facade.
TEST_F(AccountProfileMapperTest, GetAccounts) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B", "C"}}});

  base::MockRepeatingCallback<void(const std::vector<Account>&)> mock_callback;

  // `GetAccounts()` does not go through the facade, but directly reads from
  // storage.
  EXPECT_CALL(*mock_facade(), GetAccounts(testing::_)).Times(0);

  // Non-existing profile.
  EXPECT_CALL(mock_callback, Run(testing::IsEmpty()));
  mapper->GetAccounts(GetProfilePath("MissingAccount"), mock_callback.Get());
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

// Test basic functionality for `GetAccountsMap()` with unassigned accounts:
// - returns the complete map, incl. unassigned accounts,
// - does not trigger a call to GetAccounts() on the facade.
TEST_F(AccountProfileMapperTest, GetAccountsMapWithUnassigned) {
  base::FilePath other_path = GetProfilePath("Other");
  base::FilePath empty_path;
  AccountProfileMapper* mapper = CreateMapper(
      {{main_path(), {"A"}}, {other_path, {"B", "C"}}, {empty_path, {"D"}}});

  // `GetAccountsMap()` does not go through the facade, but directly reads from
  // storage.
  EXPECT_CALL(*mock_facade(), GetAccounts(testing::_)).Times(0);

  base::MockRepeatingCallback<void(
      const std::map<base::FilePath, std::vector<account_manager::Account>>&)>
      mock_callback;
  EXPECT_CALL(
      mock_callback,
      Run(testing::UnorderedElementsAre(
          testing::Pair(main_path(),
                        testing::UnorderedElementsAre(
                            Field(&Account::key, AccountKey{"A", kGaiaType}))),
          testing::Pair(other_path,
                        testing::UnorderedElementsAre(
                            Field(&Account::key, AccountKey{"B", kGaiaType}),
                            Field(&Account::key, AccountKey{"C", kGaiaType}))),
          testing::Pair(empty_path,
                        testing::UnorderedElementsAre(Field(
                            &Account::key, AccountKey{"D", kGaiaType}))))));
  mapper->GetAccountsMap(mock_callback.Get());
}

// Test basic functionality for `GetAccountsMap()` without unassigned accounts:
// - returns the complete map, without the entry for unassigned accounts,
// - does not trigger a call to GetAccounts() on the facade.
TEST_F(AccountProfileMapperTest, GetAccountsMapWithoutUnassigned) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B", "C"}}});

  // `GetAccountsMap()` does not go through the facade, but directly reads from
  // storage.
  EXPECT_CALL(*mock_facade(), GetAccounts(testing::_)).Times(0);

  base::MockRepeatingCallback<void(
      const std::map<base::FilePath, std::vector<account_manager::Account>>&)>
      mock_callback;
  EXPECT_CALL(
      mock_callback,
      Run(testing::UnorderedElementsAre(
          testing::Pair(main_path(),
                        testing::UnorderedElementsAre(
                            Field(&Account::key, AccountKey{"A", kGaiaType}))),
          testing::Pair(
              other_path,
              testing::UnorderedElementsAre(
                  Field(&Account::key, AccountKey{"B", kGaiaType}),
                  Field(&Account::key, AccountKey{"C", kGaiaType}))))));
  mapper->GetAccountsMap(mock_callback.Get());
}

// Tests that accounts are added by default to the main profile when there is
// only one profile.
TEST_F(AccountProfileMapperTest, UpdateSingleProfile) {
  AccountProfileMapper* mapper = CreateMapper({{main_path(), {"A", "B"}}});
  TestMapperUpdateGaia(mapper,
                       /*accounts_in_facade=*/{"A", "C"},
                       /*expected_accounts_upserted=*/{{main_path(), {"C"}}},
                       /*expected_accounts_removed=*/{{main_path(), {"B"}}},
                       /*expected_accounts_in_prefs=*/
                       {{main_path(), {"A", "C"}}});
}

// Tests that at AccountProfileMapper initialization when there is only one
// profile:
// - a new account is added to the main profile storage
// - a no longer existing account is removed form the profile storage
TEST_F(AccountProfileMapperTest,
       UpdateSingleProfile_AtInitialization_EmptyLocalState) {
  CreateMapperNonInitialized({{main_path(), {"A", "B"}}});
  // Clean local state.
  SetLacrosAccountsInLocalState({});
  // B is removed and C is added.
  CompleteFacadeGetAccountsGaia({"A", "C"});
  VerifyAccountsInPrefs({{main_path(), {"A", "C"}}});
}

// Tests that at AccountProfileMapper initialization when there is only one
// profile:
// - an unassigned account is not added to the main profile storage
// - a no longer existing account is removed from the profile storage
TEST_F(AccountProfileMapperTest, UpdateSingleProfile_AtInitialization) {
  CreateMapperNonInitialized(
      {{main_path(), {"A", "B"}}, {base::FilePath(), {"C"}}});
  // B is removed.
  CompleteFacadeGetAccountsGaia({"A", "C"});
  VerifyAccountsInPrefs({{main_path(), {"A"}}, {base::FilePath(), {"C"}}});
}

// Tests that new accounts are left unassigned when there are multiple profiles.
TEST_F(AccountProfileMapperTest, UpdateMultiProfile) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  TestMapperUpdateGaia(
      mapper,
      /*accounts_in_facade=*/{"A", "B", "D"},
      /*expected_accounts_upserted=*/{{base::FilePath(), {"D"}}},
      /*expected_accounts_removed=*/{{other_path, {"C"}}},
      /*expected_accounts_in_prefs=*/
      {{main_path(), {"A"}}, {other_path, {"B"}}, {base::FilePath(), {"D"}}});
}

// Tests that at AccountProfileMapper initialization when there are multiple
// profiles:
// - a new account is not added to the main profile storage
// - a no longer existing account is removed form the profile storage
TEST_F(AccountProfileMapperTest,
       UpdateMultiProfile_AtInitialization_EmptyLocalState) {
  base::FilePath other_path = GetProfilePath("Other");
  CreateMapperNonInitialized({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  // Clean local state.
  SetLacrosAccountsInLocalState({});
  // C is removed and D is added.
  CompleteFacadeGetAccountsGaia({"A", "B", "D"});
  VerifyAccountsInPrefs(
      {{main_path(), {"A"}}, {other_path, {"B"}}, {base::FilePath(), {"D"}}});
}

// Tests that at AccountProfileMapper initialization when there are multiple
// profiles:
// - an unassigned account is not added to the main profile storage
// - a no longer existing account is removed from the profile storage
TEST_F(AccountProfileMapperTest, UpdateMultiProfile_AtInitialization) {
  base::FilePath other_path = GetProfilePath("Other");
  CreateMapperNonInitialized({{main_path(), {"A"}},
                              {other_path, {"B", "C"}},
                              {base::FilePath(), {"D"}}});
  // C is removed.
  CompleteFacadeGetAccountsGaia({"A", "B", "D"});
  VerifyAccountsInPrefs(
      {{main_path(), {"A"}}, {other_path, {"B"}}, {base::FilePath(), {"D"}}});
}

// Checks that `GetPersistentErrorForAccount()` returns an error when the
// account is not in this profile.
TEST_F(AccountProfileMapperTest, GetPersistentErrorForAccount) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B"}}});
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
  AccountProfileMapper* mapper =
      CreateMapperNonInitialized({{main_path(), {"A", "B"}}});
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
  AccountProfileMapper* mapper =
      CreateMapperNonInitialized({{main_path(), {"A"}}});
  // Change the prefs, so that observers would normally trigger.
  SetAccountsInStorage({{main_path(), {"A", "B"}}});
  SetLacrosAccountsInLocalState({"A", "B"});

  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);

  // Observers were not called even though the storage was updated.
  VerifyAccountsInPrefs({{main_path(), {"A", "B"}}});
  CompleteFacadeGetAccountsGaia({"A"});
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  VerifyAccountsInPrefs({{main_path(), {"A"}}});
}

TEST_F(AccountProfileMapperTest, NonGaia) {
  AccountProfileMapper* mapper = CreateMapper({{main_path(), {"A"}}});
  // Addition of non-Gaia account is ignored.
  TestMapperUpdate(mapper, {AccountFromGaiaID("A"), NonGaiaAccountFromID("B")},
                   /*expected_accounts_upserted=*/{},
                   /*expected_accounts_removed=*/{},
                   /*expected_accounts_in_prefs=*/{{main_path(), {"A"}}});
  // Removal is ignored as well.
  TestMapperUpdate(mapper, {AccountFromGaiaID("A")},
                   /*expected_accounts_upserted=*/{},
                   /*expected_accounts_removed=*/{},
                   /*expected_accounts_in_prefs=*/{{main_path(), {"A"}}});
}

// Tests that observers are notified when an existing account receives an
// update.
TEST_F(AccountProfileMapperTest, ObserveAccountUpdate) {
  base::FilePath second_path = GetProfilePath("Second");
  base::FilePath third_path = GetProfilePath("Third");
  AccountProfileMapper* mapper = CreateMapper(
      {{main_path(), {"A"}}, {second_path, {"B"}}, {third_path, {"A", "B"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);

  ExpectOnAccountUpserted(&mock_observer,
                          {{main_path(), {"A"}}, {third_path, {"A"}}});
  mapper->OnAccountUpserted(AccountFromGaiaID("A"));
}

// Tests that observers are not notified when a non-Gaia account receives an
// update.
TEST_F(AccountProfileMapperTest, ObserveAccountUpdate_NonGaia) {
  base::FilePath second_path = GetProfilePath("Second");
  base::FilePath third_path = GetProfilePath("Third");
  AccountProfileMapper* mapper = CreateMapper(
      {{main_path(), {"A"}}, {second_path, {"B"}}, {third_path, {"A", "B"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);

  ExpectOnAccountUpserted(&mock_observer, {});
  mapper->OnAccountUpserted(NonGaiaAccountFromID("A"));
}

// Tests that observers are notified when an existing unassigned Gaia account
// receives an update.
TEST_F(AccountProfileMapperTest, ObserveAccountUpdate_Unassigned) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper = CreateMapper({{main_path(), {"A", "B"}},
                                               {other_path, {"B"}},
                                               {base::FilePath(), {"C"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);

  ExpectOnAccountUpserted(&mock_observer, {{base::FilePath(), {"C"}}});
  mapper->OnAccountUpserted(AccountFromGaiaID("C"));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  TestMapperUpdateGaia(
      mapper,
      /*accounts_in_facade=*/{"A", "B"},
      /*expected_accounts_upserted=*/{},
      /*expected_accounts_removed=*/{{base::FilePath(), {"C"}}},
      /*expected_accounts_in_prefs=*/
      {{main_path(), {"A", "B"}}, {other_path, {"B"}}});
}

// Tests that observers are notified when an existing account receives an
// update before the AccountProfileMapper was initialized.
TEST_F(AccountProfileMapperTest, ObserveAccountUpdate_AtInitialization) {
  base::FilePath second_path = GetProfilePath("Second");
  base::FilePath third_path = GetProfilePath("Third");
  AccountProfileMapper* mapper = CreateMapperNonInitialized(
      {{main_path(), {"A"}}, {second_path, {"B"}}, {third_path, {"A", "B"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);

  EXPECT_CALL(*mock_facade(), GetAccounts(testing::_)).Times(0);
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  mapper->OnAccountUpserted(AccountFromGaiaID("A"));
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  ExpectOnAccountUpserted(&mock_observer,
                          {{main_path(), {"A"}}, {third_path, {"A"}}});
  CompleteFacadeGetAccountsGaia({"A", "B"});
}

// Tests that observers are notified in the edge-case scenario when an account
// is removed and instantly re-added to the system.
TEST_F(AccountProfileMapperTest, ObserveAccountReadded) {
  base::FilePath second_path = GetProfilePath("Second");
  base::FilePath third_path = GetProfilePath("Third");
  AccountProfileMapper* mapper = CreateMapper({{main_path(), {"A", "B"}},
                                               {second_path, {"A"}},
                                               {third_path, {"A", "B"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);

  ExpectFacadeGetAccountsCalled();
  mapper->OnAccountRemoved(AccountFromGaiaID("B"));

  // Account B gets re-added before `mapper` receives GetAccounts() callback.
  ExpectOnAccountUpserted(&mock_observer,
                          {{main_path(), {"B"}}, {third_path, {"B"}}});
  mapper->OnAccountUpserted(AccountFromGaiaID("B"));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // The callback triggered by `OnAccountRemoved()` returns stale data that
  // contains only one account.
  ExpectOnAccountRemoved(&mock_observer,
                         {{main_path(), {"B"}}, {third_path, {"B"}}});
  CompleteFacadeGetAccountsGaia({"A"});
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Account B gets re-added as an unassigned account.
  ExpectOnAccountUpserted(&mock_observer, {{base::FilePath(), {"B"}}});
  CompleteFacadeGetAccountsGaia({"A", "B"});
}

// Tests that observers are notified about changes to accounts' error status.
TEST_F(AccountProfileMapperTest, ObserveAuthErrorChanged) {
  base::FilePath second_path = GetProfilePath("Second");
  base::FilePath third_path = GetProfilePath("Third");
  AccountProfileMapper* mapper = CreateMapper(
      {{main_path(), {"A", "B"}}, {second_path, {"A"}}, {third_path, {"B"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);

  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  ExpectOnAuthErrorChanged(&mock_observer,
                           {{main_path(), {std::make_pair("A", error)}},
                            {second_path, {std::make_pair("A", error)}}});
  mapper->OnAuthErrorChanged(account_manager::AccountKey{"A", kGaiaType},
                             error);
}

// Tests that a managed syncing secondary profile gets deleted after its primary
// account is removed from the system.
// A secondary account of the deleted profile stays unassigned.
TEST_F(AccountProfileMapperTest,
       RemovePrimaryAccountFromSecondaryProfile_MultipleProfiles) {
  base::FilePath second_path = GetProfilePath("Second");
  base::FilePath third_path = GetProfilePath("Third");
  AccountProfileMapper* mapper = CreateMapper(
      {{main_path(), {"A"}}, {second_path, {"B", "C"}}, {third_path, {"D"}}});
  SetPrimaryAccountForProfile(second_path, "B",
                              /*is_consented_primary_account=*/true,
                              /*is_managed=*/true);
  TestMapperUpdateGaia(
      mapper,
      /*accounts_in_facade=*/{"A", "C", "D"},
      /*expected_accounts_upserted=*/{},
      /*expected_accounts_removed=*/{{second_path, {"B", "C"}}},
      /*expected_accounts_in_prefs=*/
      {{main_path(), {"A"}}, {third_path, {"D"}}, {base::FilePath(), {"C"}}});
  ProfileAttributesStorageTestObserver(attributes_storage())
      .WaitForProfileBeingDeleted(second_path);
}

// Local profiles are not deleted.
TEST_F(AccountProfileMapperTest, LocalProfileNotRemoved) {
  base::FilePath second_path = GetProfilePath("Second");
  base::FilePath third_path = GetProfilePath("Third");
  AccountProfileMapper* mapper = CreateMapper(
      {{main_path(), {"A"}}, {second_path, {"B"}}, {third_path, {}}});
  SetPrimaryAccountForProfile(second_path, "B",
                              /*is_consented_primary_account=*/true,
                              /*is_managed=*/true);
  TestMapperUpdateGaia(mapper,
                       /*accounts_in_facade=*/{"A"},
                       /*expected_accounts_upserted=*/{},
                       /*expected_accounts_removed=*/{{second_path, {"B"}}},
                       /*expected_accounts_in_prefs=*/
                       {{main_path(), {"A"}}, {third_path, {}}});

  ProfileAttributesStorageTestObserver(attributes_storage())
      .WaitForProfileBeingDeleted(second_path);

  // Third profile was not deleted as it was already a local profile.
  EXPECT_TRUE(attributes_storage()->GetProfileAttributesWithPath(third_path));
}

// Tests that a managed syncing profile gets deleted after its sync account
// is removed from the system. A secondary account of the deleted profile stays
// unassigned.
TEST_F(AccountProfileMapperTest,
       RemovePrimaryAccount_ManagedSecondaryProfile_Syncing) {
  base::FilePath second_path = GetProfilePath("Second");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {second_path, {"B"}}});
  SetPrimaryAccountForProfile(second_path, "B",
                              /*is_consented_primary_account=*/true,
                              /*is_managed=*/true);
  TestMapperUpdateGaia(mapper,
                       /*accounts_in_facade=*/{"A"},
                       /*expected_accounts_upserted=*/{},
                       /*expected_accounts_removed=*/{{second_path, {"B"}}},
                       /*expected_accounts_in_prefs=*/{{main_path(), {"A"}}});

  ProfileAttributesStorageTestObserver(attributes_storage())
      .WaitForProfileBeingDeleted(second_path);
}

// Tests that a managed non syncing profile does not get deleted after its
// primary account is removed from the system.
TEST_F(AccountProfileMapperTest,
       RemovePrimaryAccount_ManagedSecondaryProfile_NotSyncing) {
  base::FilePath second_path = GetProfilePath("Second");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {second_path, {"B"}}});
  SetPrimaryAccountForProfile(second_path, "B",
                              /*is_consented_primary_account=*/false,
                              /*is_managed=*/true);
  TestMapperUpdateGaia(mapper,
                       /*accounts_in_facade=*/{"A"},
                       /*expected_accounts_upserted=*/{},
                       /*expected_accounts_removed=*/{{second_path, {"B"}}},
                       /*expected_accounts_in_prefs=*/
                       {{main_path(), {"A"}}, {second_path, {}}});

  base::RunLoop().RunUntilIdle();
  // Only managed syncing profiles are deleted.
  EXPECT_TRUE(attributes_storage()->GetProfileAttributesWithPath(second_path));
}

// Tests that a consumer profile does not get deleted after its sync account
// is removed from the system.
TEST_F(AccountProfileMapperTest,
       RemovePrimaryAccount_ConsumerSecondaryProfile) {
  base::FilePath second_path = GetProfilePath("Second");
  base::FilePath third_path = GetProfilePath("Third");
  AccountProfileMapper* mapper = CreateMapper(
      {{main_path(), {"A"}}, {second_path, {"B", "C"}}, {third_path, {"D"}}});
  SetPrimaryAccountForProfile(second_path, "B");
  TestMapperUpdateGaia(
      mapper,
      /*accounts_in_facade=*/{"A", "C", "D"},
      /*expected_accounts_upserted=*/{},
      /*expected_accounts_removed=*/{{second_path, {"B"}}},
      /*expected_accounts_in_prefs=*/
      {{main_path(), {"A"}}, {second_path, {"C"}}, {third_path, {"D"}}});

  base::RunLoop().RunUntilIdle();
  // Only managed syncing profiles are deleted.
  // The `SigninManager` will detect as soon the second profile is loaded that
  // its primary account does not have a refresh token and will completely
  // signout the profile.
  EXPECT_TRUE(attributes_storage()->GetProfileAttributesWithPath(second_path));
  EXPECT_TRUE(attributes_storage()->GetProfileAttributesWithPath(third_path));
}

// Tests that a manged syncing secondary profile gets deleted after its sync
// account was removed from the system before startup.
// A secondary account of the deleted profile gets moved to the primary profile
// since local state doesn't contain lacros accounts and there is only one
// profile left.
TEST_F(
    AccountProfileMapperTest,
    RemovePrimaryAccountFromSecondaryProfile_AtInitialization_EmptyLocalState) {
  base::FilePath other_path = GetProfilePath("Other");
  CreateMapperNonInitialized({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  // Clean local state.
  SetLacrosAccountsInLocalState({});
  SetPrimaryAccountForProfile(other_path, "B",
                              /*is_consented_primary_account=*/true,
                              /*is_managed=*/true);
  CompleteFacadeGetAccountsGaia({"A", "C"});
  VerifyAccountsInPrefs({{main_path(), {"A", "C"}}});
  ProfileAttributesStorageTestObserver(attributes_storage())
      .WaitForProfileBeingDeleted(other_path);
}

// Tests that a managed secondary profile gets deleted after its sync account
//  was removed from the system before startup.
// A secondary account of the deleted profile remains unassigned.
TEST_F(AccountProfileMapperTest,
       RemovePrimaryAccountFromSecondaryProfile_AtInitialization) {
  base::FilePath other_path = GetProfilePath("Other");
  CreateMapperNonInitialized({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  SetPrimaryAccountForProfile(other_path, "B",
                              /*is_consented_primary_account=*/true,
                              /*is_managed=*/true);
  CompleteFacadeGetAccountsGaia({"A", "C"});
  VerifyAccountsInPrefs({{main_path(), {"A"}}, {base::FilePath(), {"C"}}});
  ProfileAttributesStorageTestObserver(attributes_storage())
      .WaitForProfileBeingDeleted(other_path);
}

// Tests that a managed syncing secondary profile doesn't get deleted after its
// secondary account is removed from the system.
TEST_F(AccountProfileMapperTest, RemoveSecondaryAccountFromSecondaryProfile) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  SetPrimaryAccountForProfile(other_path, "B",
                              /*is_consented_primary_account=*/true,
                              /*is_managed=*/true);
  TestMapperUpdateGaia(mapper,
                       /*accounts_in_facade=*/{"A", "B"},
                       /*expected_accounts_upserted=*/{},
                       /*expected_accounts_removed=*/{{other_path, {"C"}}},
                       /*expected_accounts_in_prefs=*/
                       {{main_path(), {"A"}}, {other_path, {"B"}}});
}

// Tests that the primary profile doesn't get deleted even after its primary
// account is removed from the system.
TEST_F(AccountProfileMapperTest, RemovePrimaryAccountFromPrimaryProfile) {
  AccountProfileMapper* mapper = CreateMapper({{main_path(), {"A", "B"}}});
  SetPrimaryAccountForProfile(main_path(), "A",
                              /*is_consented_primary_account=*/true,
                              /*is_managed=*/true);
  TestMapperUpdateGaia(mapper,
                       /*accounts_in_facade=*/{"B"},
                       /*expected_accounts_upserted=*/{},
                       /*expected_accounts_removed=*/{{main_path(), {"A"}}},
                       /*expected_accounts_in_prefs=*/{{main_path(), {"B"}}});
}

// Tests removing all accounts from a secondary profile (User signed out from
// chrome or primary account removed from the OS) before initialization.
TEST_F(AccountProfileMapperTest,
       RemoveAllAccountsFromSecondaryProfile_BeforeInitialization) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper = CreateMapperNonInitialized(
      {{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  ExpectOnAccountRemoved(&mock_observer, {{other_path, {"B", "C"}}});
  mapper->RemoveAllAccounts(other_path);
  CompleteFacadeGetAccountsGaia({"A", "B", "C"});
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  VerifyAccountsInStorage({{main_path(), {"A"}}, {other_path, {}}});
}

// Tests removing all accounts from a secondary profile and account removed from
// the OS before initialization.
TEST_F(
    AccountProfileMapperTest,
    RemoveAllAccountsFromSecondaryProfile_OSAccountsChanged_BeforeInitialization) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper = CreateMapperNonInitialized(
      {{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  // "C" is removed from the ProfileAttributeEntry by RemoveStaleAccounts.
  ExpectOnAccountRemoved(&mock_observer, {{other_path, {"B"}}});
  mapper->RemoveAllAccounts(other_path);
  CompleteFacadeGetAccountsGaia({"A", "B"});
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  VerifyAccountsInStorage({{main_path(), {"A"}}, {other_path, {}}});
}

// Tests removing all accounts from a secondary profile.
TEST_F(AccountProfileMapperTest, RemoveAllAccountsFromSecondaryProfile) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  ExpectOnAccountRemoved(&mock_observer, {{other_path, {"B", "C"}}});
  mapper->RemoveAllAccounts(other_path);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  VerifyAccountsInStorage({{main_path(), {"A"}}, {other_path, {}}});
}

// Tests removing all accounts from main profile is not allowed.
TEST_F(AccountProfileMapperTest, RemoveAllAccountsFromPrimaryProfile) {
  AccountProfileMapper* mapper = CreateMapper({{main_path(), {"A", "B"}}});
  SetPrimaryAccountForProfile(main_path(), "A");
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  ExpectOnAccountRemoved(&mock_observer, {});
  mapper->RemoveAllAccounts(main_path());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  VerifyAccountsInStorage({{main_path(), {"A", "B"}}});
}

// Tests removing accounts from secondary profile.
TEST_F(AccountProfileMapperTest, RemoveAccountSecondaryProfile) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  SetPrimaryAccountForProfile(other_path, "B");
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  // Remove account C (secondary account).
  ExpectOnAccountRemoved(&mock_observer, {{other_path, {"C"}}});
  mapper->RemoveAccount(other_path, AccountFromGaiaID("C").key);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  VerifyAccountsInStorage({{main_path(), {"A"}}, {other_path, {"B"}}});
  // Remove account B (main account).
  ExpectOnAccountRemoved(&mock_observer, {{other_path, {"B"}}});
  mapper->RemoveAccount(other_path, AccountFromGaiaID("B").key);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  VerifyAccountsInStorage({{main_path(), {"A"}}, {other_path, {}}});
}

// Tests removing accounts from main profile.
TEST_F(AccountProfileMapperTest, RemoveAccountPrimaryProfile) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A", "B"}}, {other_path, {"B"}}});
  SetPrimaryAccountForProfile(main_path(), "A");
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  // Remove account B.
  ExpectOnAccountRemoved(&mock_observer, {{main_path(), {"B"}}});
  mapper->RemoveAccount(main_path(), AccountFromGaiaID("B").key);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  VerifyAccountsInStorage({{main_path(), {"A"}}, {other_path, {"B"}}});
  // Try removing account A: this does nothing.
  mapper->RemoveAccount(main_path(), AccountFromGaiaID("A").key);
  VerifyAccountsInStorage({{main_path(), {"A"}}, {other_path, {"B"}}});
}

// Tests removing all accounts from profile before initialization but profile
// is deleted during initialization.
TEST_F(
    AccountProfileMapperTest,
    RemoveAllAccountsFromSecondaryProfile_ProfileDeletedDuringInitialization) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper = CreateMapperNonInitialized(
      {{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  SetPrimaryAccountForProfile(other_path, "B",
                              /*is_consented_primary_account=*/true,
                              /*is_managed=*/true);
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  ExpectOnAccountRemoved(&mock_observer, {{other_path, {"C"}}});
  mapper->RemoveAllAccounts(other_path);
  CompleteFacadeGetAccountsGaia({"A", "C"});
  VerifyAccountsInPrefs({{main_path(), {"A"}}, {base::FilePath(), {"C"}}});
  ProfileAttributesStorageTestObserver(attributes_storage())
      .WaitForProfileBeingDeleted(other_path);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
}

// Tests that accounts from deleted profile remain unassigned.
TEST_F(AccountProfileMapperTest, DeleteProfile) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B", "C"}}});

  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  ExpectOnAccountRemoved(&mock_observer, {{other_path, {"B", "C"}}});

  testing_profile_manager()->DeleteTestingProfile("Other");
  VerifyAccountsInPrefs({{main_path(), {"A"}}, {base::FilePath(), {"B", "C"}}});
}

TEST_F(AccountProfileMapperTest, ShowAddAccountDialogBeforeInit) {
  AccountProfileMapper* mapper =
      CreateMapperNonInitialized({{main_path(), {"A"}}});
  AccountManagerFacade::AccountAdditionSource source =
      AccountManagerFacade::AccountAdditionSource::kOgbAddAccount;
  // The facade is not called before initialization.
  EXPECT_CALL(*mock_facade(), ShowAddAccountDialog(testing::_, testing::_))
      .Times(0);

  mapper->ShowAddAccountDialog(main_path(), source,
                               AccountProfileMapper::AddAccountCallback());
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  // Complete initialization, and check that the facade was called.
  EXPECT_CALL(*mock_facade(), ShowAddAccountDialog(source, testing::_));
  CompleteFacadeGetAccountsGaia({"A"});
  testing::Mock::VerifyAndClearExpectations(mock_facade());
}

TEST_F(AccountProfileMapperTest, ShowAddAccountDialog) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B"}}});

  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);

  MockAddAccountCallback account_added_callback;
  Account account_c = AccountFromGaiaID("C");
  std::optional<AccountProfileMapper::AddAccountResult> result =
      AccountProfileMapper::AddAccountResult{other_path, account_c};
  AccountManagerFacade::AccountAdditionSource source =
      AccountManagerFacade::AccountAdditionSource::kOgbAddAccount;

  // Success: Add account to existing profile.
  ExpectFacadeShowAddAccountDialogCalled(source, account_c);
  ExpectFacadeGetAccountsCalled();
  EXPECT_CALL(account_added_callback, Run(AddAccountResultEqual(result)));
  EXPECT_CALL(mock_observer,
              OnAccountUpserted(other_path, AccountEqual(account_c)));
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);
  mapper->ShowAddAccountDialog(other_path, source,
                               account_added_callback.Get());
  mapper->OnAccountUpserted(account_c);
  // The first `GetAccounts()` call is generated by `OnAccountUpserted()`.
  CompleteFacadeGetAccountsGaia({"A", "B", "C"});
  // The second `GetAccounts()` call is generated when an `AddAccountHelper`
  // completes.
  CompleteFacadeGetAccountsGaia({"A", "B", "C"});
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  VerifyAccountsInPrefs({{main_path(), {"A"}}, {other_path, {"B", "C"}}});

  // Success: Add account to existing profile (with no callback provided).
  Account account_d = AccountFromGaiaID("D");
  ExpectFacadeShowAddAccountDialogCalled(source, account_d);
  ExpectFacadeGetAccountsCalled();
  EXPECT_CALL(mock_observer,
              OnAccountUpserted(other_path, AccountEqual(account_d)));
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);
  mapper->ShowAddAccountDialog(other_path, source,
                               AccountProfileMapper::AddAccountCallback());
  mapper->OnAccountUpserted(account_d);
  // The first `GetAccounts()` call is generated by `OnAccountUpserted()`.
  CompleteFacadeGetAccountsGaia({"A", "B", "C", "D"});
  // The second `GetAccounts()` call is generated when an `AddAccountHelper`
  // completes.
  CompleteFacadeGetAccountsGaia({"A", "B", "C", "D"});
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  VerifyAccountsInPrefs({{main_path(), {"A"}}, {other_path, {"B", "C", "D"}}});

  // Failure: Add account that already exists.
  ExpectFacadeShowAddAccountDialogCalled(source, account_c);
  EXPECT_CALL(account_added_callback, Run(testing::Eq(std::nullopt)));
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);
  mapper->ShowAddAccountDialog(other_path, source,
                               account_added_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Failure: Add account to non-existing profile, account is unassigned.
  Account account_e = AccountFromGaiaID("E");
  base::FilePath unknown_path = GetProfilePath("UnknownProfile");
  ExpectFacadeShowAddAccountDialogCalled(source, account_e);
  ExpectFacadeGetAccountsCalled();
  result = {base::FilePath(), account_e};
  EXPECT_CALL(account_added_callback, Run(AddAccountResultEqual(result)));
  EXPECT_CALL(mock_observer,
              OnAccountUpserted(base::FilePath(), AccountEqual(account_e)));
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);
  mapper->ShowAddAccountDialog(unknown_path, source,
                               account_added_callback.Get());
  mapper->OnAccountUpserted(account_e);
  // The first `GetAccounts()` call is generated by `OnAccountUpserted()`.
  CompleteFacadeGetAccountsGaia({"A", "B", "C", "D", "E"});
  // The second `GetAccounts()` call is generated when an `AddAccountHelper`
  // completes.
  CompleteFacadeGetAccountsGaia({"A", "B", "C", "D", "E"});
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  // `account_e` is added as an unassigned account.
  VerifyAccountsInPrefs({{main_path(), {"A"}},
                         {other_path, {"B", "C", "D"}},
                         {base::FilePath(), {"E"}}});

  // Failure: Non-Gaia account.
  Account account_f = NonGaiaAccountFromID("F");
  ExpectFacadeShowAddAccountDialogCalled(source, account_f);
  EXPECT_CALL(account_added_callback, Run(testing::Eq(std::nullopt)));
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);
  mapper->ShowAddAccountDialog(other_path, source,
                               account_added_callback.Get());
  mapper->OnAccountUpserted(account_f);

  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Failure: Flow aborted.
  ExpectFacadeShowAddAccountDialogCalled(source, std::nullopt);
  EXPECT_CALL(account_added_callback, Run(testing::Eq(std::nullopt)));
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);
  mapper->ShowAddAccountDialog(other_path, source,
                               account_added_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // No account was assigned by any the failures above.
  VerifyAccountsInPrefs({{main_path(), {"A"}},
                         {other_path, {"B", "C", "D"}},
                         {base::FilePath(), {"E"}}});
}

// Tests that an account is fully added only after the account manager called
// both the account added callback and the `OnAccountUpserted()` method.
// The account added callback is called first in this test.
TEST_F(AccountProfileMapperTest,
       ShowAddAccountDialogTwoPhase_AccountAddedCallbackFirst) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  MockAddAccountCallback account_added_callback;
  Account account_c = AccountFromGaiaID("C");
  std::optional<AccountProfileMapper::AddAccountResult> result =
      AccountProfileMapper::AddAccountResult{other_path, account_c};
  AccountManagerFacade::AccountAdditionSource source =
      AccountManagerFacade::AccountAdditionSource::kOgbAddAccount;

  // No events fire before the account manager upserts an account:
  EXPECT_CALL(account_added_callback, Run(testing::_)).Times(0);
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  ExpectFacadeShowAddAccountDialogCalled(source, account_c);
  mapper->ShowAddAccountDialog(other_path, source,
                               account_added_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  VerifyAccountsInPrefs({{main_path(), {"A"}}, {other_path, {"B"}}});

  // An account is added after the account manager upserts it:
  EXPECT_CALL(account_added_callback, Run(AddAccountResultEqual(result)));
  EXPECT_CALL(mock_observer,
              OnAccountUpserted(other_path, AccountEqual(account_c)));
  ExpectFacadeGetAccountsCalled();
  mapper->OnAccountUpserted(account_c);
  // The first `GetAccounts()` call is generated by `OnAccountUpserted()`.
  CompleteFacadeGetAccountsGaia({"A", "B", "C"});
  // The second `GetAccounts()` call is generated when an `AddAccountHelper`
  // completes.
  CompleteFacadeGetAccountsGaia({"A", "B", "C"});
  VerifyAccountsInPrefs({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
}

// Tests that an account is fully added only after the account manager called
// both the account added callback and the `OnAccountUpserted()` method.
// The `OnAccountUpserted()` method is called first in this test.
TEST_F(AccountProfileMapperTest,
       ShowAddAccountDialogTwoPhase_AccountUpsertedFirst) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  MockAddAccountCallback account_added_callback;
  Account account_c = AccountFromGaiaID("C");
  std::optional<AccountProfileMapper::AddAccountResult> result =
      AccountProfileMapper::AddAccountResult{other_path, account_c};
  AccountManagerFacade::AccountAdditionSource source =
      AccountManagerFacade::AccountAdditionSource::kOgbAddAccount;
  base::OnceCallback<void(const AccountUpsertionResult&)>
      show_add_account_dialog_facade_callback;

  // No events fire before the account manager invokes the account added
  // callback:
  EXPECT_CALL(account_added_callback, Run(testing::_)).Times(0);
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_facade(), ShowAddAccountDialog(source, testing::_))
      .WillOnce([&show_add_account_dialog_facade_callback](
                    AccountManagerFacade::AccountAdditionSource,
                    base::OnceCallback<void(const AccountUpsertionResult&)>
                        callback) {
        show_add_account_dialog_facade_callback = std::move(callback);
      });
  ExpectFacadeGetAccountsCalled();
  mapper->ShowAddAccountDialog(other_path, source,
                               account_added_callback.Get());
  mapper->OnAccountUpserted(account_c);
  CompleteFacadeGetAccountsGaia({"A", "B", "C"});
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_TRUE(show_add_account_dialog_facade_callback);
  VerifyAccountsInPrefs({{main_path(), {"A"}}, {other_path, {"B"}}});

  // An account is added after the account manager invokes the account added
  // callback:
  EXPECT_CALL(account_added_callback, Run(AddAccountResultEqual(result)));
  EXPECT_CALL(mock_observer,
              OnAccountUpserted(other_path, AccountEqual(account_c)));
  ExpectFacadeGetAccountsCalled();
  std::move(show_add_account_dialog_facade_callback)
      .Run(AccountUpsertionResult::FromAccount(account_c));
  // `mapper` updates the account list after it adds an account.
  CompleteFacadeGetAccountsGaia({"A", "B", "C"});
  VerifyAccountsInPrefs({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
}

TEST_F(AccountProfileMapperTest, ShowAddAccountDialogNewProfile) {
  AccountProfileMapper* mapper = CreateMapper({{main_path(), {"A"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  MockAddAccountCallback account_added_callback;

  // Set expectations: a new profile should be created for the new account.
  AccountManagerFacade::AccountAdditionSource source =
      AccountManagerFacade::AccountAdditionSource::kChromeProfileCreation;
  Account account_b = AccountFromGaiaID("B");
  ExpectFacadeShowAddAccountDialogCalled(source, account_b);
  ExpectFacadeGetAccountsCalled();
  base::FilePath new_profile_path = GetProfilePath("Profile 1");
  std::optional<AccountProfileMapper::AddAccountResult> result =
      AccountProfileMapper::AddAccountResult{new_profile_path, account_b};
  EXPECT_CALL(account_added_callback, Run(AddAccountResultEqual(result)));
  EXPECT_CALL(mock_observer,
              OnAccountUpserted(new_profile_path, AccountEqual(account_b)));
  ProfileWaiter profile_waiter;

  // Create the profile.
  mapper->ShowAddAccountDialogAndCreateNewProfile(source,
                                                  account_added_callback.Get());
  mapper->OnAccountUpserted(account_b);
  // The first `GetAccounts()` call is generated by `OnAccountUpserted()`.
  CompleteFacadeGetAccountsGaia({"A", "B"});
  Profile* new_profile = profile_waiter.WaitForProfileAdded();
  // The second `GetAccounts()` call is generated when an `AddAccountHelper`
  // completes after a new profile is created.
  CompleteFacadeGetAccountsGaia({"A", "B"});

  // Check that the profile was created and configured.
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_EQ(new_profile->GetPath(), new_profile_path);
  VerifyAccountsInPrefs({{main_path(), {"A"}}, {new_profile_path, {"B"}}});
  ProfileAttributesEntry* entry =
      attributes_storage()->GetProfileAttributesWithPath(new_profile_path);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->IsOmitted());
  EXPECT_TRUE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetSigninState(), SigninState::kNotSignedIn);
}

TEST_F(AccountProfileMapperTest, AddAccount) {
  // Start with account "C" unassigned.
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper = CreateMapper(
      {{main_path(), {"A"}}, {other_path, {"B"}}, {base::FilePath(), {"C"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  MockAddAccountCallback account_added_callback;

  // Set expectations.
  Account account_c = AccountFromGaiaID("C");
  std::optional<AccountProfileMapper::AddAccountResult> result =
      AccountProfileMapper::AddAccountResult{main_path(), account_c};
  EXPECT_CALL(account_added_callback, Run(AddAccountResultEqual(result)));
  EXPECT_CALL(mock_observer,
              OnAccountUpserted(main_path(), AccountEqual(account_c)));
  EXPECT_CALL(*mock_facade(), ShowAddAccountDialog(testing::_, testing::_))
      .Times(0);

  // Add the account.
  mapper->AddAccount(main_path(), account_c.key, account_added_callback.Get());

  // Check that the account was added.
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  VerifyAccountsInPrefs({{main_path(), {"A", "C"}}, {other_path, {"B"}}});

  // Failure: Non-Gaia account (with no callback provided).
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);
  mapper->AddAccount(main_path(), NonGaiaAccountFromID("D").key,
                     AccountProfileMapper::AddAccountCallback());
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Failure: Non-existing account (with no callback provided).
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);
  mapper->AddAccount(main_path(), AccountFromGaiaID("E").key,
                     AccountProfileMapper::AddAccountCallback());
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  VerifyAccountsInPrefs({{main_path(), {"A", "C"}}, {other_path, {"B"}}});
}

// Tries adding an account "B" to the profile, when the account "B" does not
// exist.
TEST_F(AccountProfileMapperTest, AddUnknownAccount) {
  AccountProfileMapper* mapper = CreateMapper({{main_path(), {"A"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  MockAddAccountCallback account_added_callback;

  // Set expectations: the operation fails.
  Account account_b = AccountFromGaiaID("B");
  EXPECT_CALL(account_added_callback, Run(testing::Eq(std::nullopt)));
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_facade(),
              ShowAddAccountDialog(
                  AccountManagerFacade::AccountAdditionSource::kOgbAddAccount,
                  testing::_))
      .Times(0);

  // Add the account.
  mapper->AddAccount(main_path(), account_b.key, account_added_callback.Get());

  // Check expectations.
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  VerifyAccountsInPrefs({{main_path(), {"A"}}});
}

TEST_F(AccountProfileMapperTest, CreateNewProfileWithAccount) {
  // Start with account "C" unassigned.
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper = CreateMapper(
      {{main_path(), {"A"}}, {other_path, {"B"}}, {base::FilePath(), {"C"}}});
  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  MockAddAccountCallback account_added_callback;

  // Set expectations: a new profile should be created for the account.
  Account account_c = AccountFromGaiaID("C");
  EXPECT_CALL(*mock_facade(), ShowAddAccountDialog(testing::_, testing::_))
      .Times(0);
  base::FilePath new_profile_path = GetProfilePath("Profile 1");
  std::optional<AccountProfileMapper::AddAccountResult> result =
      AccountProfileMapper::AddAccountResult{new_profile_path, account_c};
  EXPECT_CALL(account_added_callback, Run(AddAccountResultEqual(result)));
  EXPECT_CALL(mock_observer,
              OnAccountUpserted(new_profile_path, AccountEqual(account_c)));
  ProfileWaiter profile_waiter;

  // Create the profile.
  mapper->CreateNewProfileWithAccount(account_c.key,
                                      account_added_callback.Get());
  Profile* new_profile = profile_waiter.WaitForProfileAdded();

  // Check that the profile was created and configured.
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_EQ(new_profile->GetPath(), new_profile_path);
  VerifyAccountsInPrefs(
      {{main_path(), {"A"}}, {other_path, {"B"}}, {new_profile_path, {"C"}}});
  ProfileAttributesEntry* entry =
      attributes_storage()->GetProfileAttributesWithPath(new_profile_path);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->IsOmitted());
  EXPECT_TRUE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetSigninState(), SigninState::kNotSignedIn);
}

TEST_F(AccountProfileMapperTest, FixProfilesAtStartupWithLocalProfiles) {
  base::FilePath syncing_path = GetProfilePath("Syncing");
  base::FilePath signed_out_path = GetProfilePath("SignedOut");
  base::FilePath unconsented_path = GetProfilePath("Unconsented");

  // Create profiles without gaia ids.
  CreateProfilesAndSetAccountsInPrefs({{main_path(), {}},
                                       {syncing_path, {}},
                                       {unconsented_path, {}},
                                       {signed_out_path, {}}});
  // Set profiles in various signin states.
  attributes_storage()
      ->GetProfileAttributesWithPath(syncing_path)
      ->SetAuthInfo(
          /*gaia_id=*/"A", /*user_name=*/u"A",
          /*is_consented_primary_account=*/true);
  attributes_storage()
      ->GetProfileAttributesWithPath(unconsented_path)
      ->SetAuthInfo(
          /*gaia_id=*/"B", /*user_name=*/u"B",
          /*is_consented_primary_account=*/false);

  auto mapper = std::make_unique<AccountProfileMapper>(
      mock_facade(), attributes_storage(), local_state());

  // The main profile is not deleted, even though it does not have an account.
  // Other profiles are not deleted either. The missing gaia IDs are added to
  // the storage.
  VerifyAccountsInStorage({{main_path(), {}},
                           {syncing_path, {"A"}},
                           {unconsented_path, {"B"}},
                           {signed_out_path, {}}});
}

// Checks that profiles are correctly imported from Ash-based Chrome.
TEST_F(AccountProfileMapperTest, MigrateAshProfile) {
  // On Ash, the accounts are not explicitly assigned to the profile in
  // `ProfileAttributesStorage`.
  CreateMapperNonInitialized(
      {{main_path(), {}}, {base::FilePath(), {"A", "B", "C"}}});
  // Local state is empty before the profile is migrated.
  SetLacrosAccountsInLocalState({});
  CompleteFacadeGetAccountsGaia({"A", "B", "C"});

  // All accounts have been assigned to the main profile.
  VerifyAccountsInPrefs({{main_path(), {"A", "B", "C"}}});
}

TEST_F(AccountProfileMapperTest, ReportAuthError) {
  base::FilePath second_path = GetProfilePath("Second");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A", "B"}}, {second_path, {"A"}}});

  const account_manager::AccountKey account_key{"A", kGaiaType};
  const GoogleServiceAuthError error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  EXPECT_CALL(*mock_facade(), ReportAuthError(account_key, error));

  mapper->ReportAuthError(second_path, account_key, error);
}

TEST_F(AccountProfileMapperTest,
       ReportAuthErrorForUnknownProfileAccountMapping) {
  base::FilePath second_path = GetProfilePath("Second");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A", "B"}}, {second_path, {"B"}}});

  const account_manager::AccountKey account_key{"A", kGaiaType};
  const GoogleServiceAuthError error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  EXPECT_CALL(*mock_facade(), ReportAuthError(account_key, error)).Times(0);

  mapper->ReportAuthError(second_path, account_key, error);
}
