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
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/test/base/profile_waiter.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_result.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
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

using MockAddAccountCallback = base::MockOnceCallback<void(
    const absl::optional<AccountProfileMapper::AddAccountResult>&)>;

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

    storage_observation_.Observe(storage_);
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
  ProfileAttributesStorage* storage_;
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      storage_observation_{this};
  base::FilePath profile_path_;
  base::RunLoop run_loop_;
};

MATCHER_P(AddAccountResultEqual,
          other,
          "optional<AddAccountResult> equality matcher") {
  if (arg == absl::nullopt && other == absl::nullopt)
    return true;
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
      : scoped_feature_list_(kMultiProfileAccountConsistency),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
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

  ProfileAttributesStorage* attributes_storage() {
    return &testing_profile_manager_.profile_manager()
                ->GetProfileAttributesStorage();
  }

  account_manager::MockAccountManagerFacade* mock_facade() {
    return &mock_facade_;
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
    ExpectAccountsInStorage(expected_accounts_in_storage);
  }

  AccountProfileMapper* CreateMapperNonInitialized(
      const AccountMapping& accounts) {
    ExpectFacadeGetAccountsCalled();
    testing_profile_manager_.SetAccountProfileMapper(
        std::make_unique<AccountProfileMapper>(mock_facade(),
                                               attributes_storage()));
    SetAccountsInStorage(accounts);
    ExpectAccountsInStorage(accounts);
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

  // Checks that the `ProfileAttributesStorage` matches `accounts_map`.
  void ExpectAccountsInStorage(const AccountMapping& accounts_map) {
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
    ASSERT_FALSE(facade_get_accounts_completion_callbacks_.empty());
    auto callback =
        std::move(facade_get_accounts_completion_callbacks_.front());
    facade_get_accounts_completion_callbacks_.pop();
    std::move(callback).Run(accounts);
  }

  // Sets the accounts in `ProfileAttributesStorage`. `accounts_map` is a map
  // from profile path to a vector of GaiaIds. One of the profiles must be the
  // main profile.
  void SetAccountsInStorage(const AccountMapping& accounts_map) {
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
    // Import accounts from the map.
    ProfileAttributesStorage* storage = attributes_storage();
    for (const auto& path_accounts_pair : accounts_map) {
      const base::FilePath path = path_accounts_pair.first;
      if (path.empty())
        continue;  // Account is unassigned.
      storage->GetProfileAttributesWithPath(path)->SetGaiaIds(
          path_accounts_pair.second);
    }
  }

  void SetPrimaryAccountForProfile(const base::FilePath& profile_path,
                                   const std::string& primary_gaia_id) {
    ProfileAttributesStorage* storage = attributes_storage();
    ProfileAttributesEntry* entry =
        storage->GetProfileAttributesWithPath(profile_path);
    ASSERT_TRUE(entry);
    entry->SetAuthInfo(primary_gaia_id, u"Test",
                       /*is_consented_primary_account=*/true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

// Tests that accounts are added by default to the main profile when there is
// only one profile.
TEST_F(AccountProfileMapperTest, UpdateSingleProfile) {
  AccountProfileMapper* mapper = CreateMapper({{main_path(), {"A", "B"}}});
  TestMapperUpdateGaia(mapper,
                       /*accounts_in_facade=*/{"A", "C"},
                       /*expected_accounts_upserted=*/{{main_path(), {"C"}}},
                       /*expected_accounts_removed=*/{{main_path(), {"B"}}},
                       /*expected_accounts_in_storage=*/
                       {{main_path(), {"A", "C"}}});
}

// Tests that new accounts are left unassigned when there are multiple profiles.
TEST_F(AccountProfileMapperTest, UpdateMulltiProfile) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  TestMapperUpdateGaia(
      mapper,
      /*accounts_in_facade=*/{"A", "B", "D"},
      /*expected_accounts_upserted=*/{{base::FilePath(), {"D"}}},
      /*expected_accounts_removed=*/{{other_path, {"C"}}},
      /*expected_accounts_in_storage=*/
      {{main_path(), {"A"}}, {other_path, {"B"}}});
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
  // Change the storage, so that observers would normally trigger.
  SetAccountsInStorage({{main_path(), {"A", "B"}}});

  MockAccountProfileMapperObserver mock_observer;
  base::ScopedObservation<AccountProfileMapper, AccountProfileMapper::Observer>
      observation{&mock_observer};
  observation.Observe(mapper);
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);

  // Observers were not called even though the storage was updated.
  ExpectAccountsInStorage({{main_path(), {"A", "B"}}});
  CompleteFacadeGetAccountsGaia({"A"});
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  ExpectAccountsInStorage({{main_path(), {"A"}}});
}

TEST_F(AccountProfileMapperTest, NonGaia) {
  AccountProfileMapper* mapper = CreateMapper({{main_path(), {"A"}}});
  // Addition of non-Gaia account is ignored.
  TestMapperUpdate(mapper, {AccountFromGaiaID("A"), NonGaiaAccountFromID("B")},
                   /*expected_accounts_upserted=*/{},
                   /*expected_accounts_removed=*/{},
                   /*expected_accounts_in_storage=*/
                   {{main_path(), {"A"}}});
  // Removal is ignored as well.
  TestMapperUpdate(mapper, {AccountFromGaiaID("A")},
                   /*expected_accounts_upserted=*/{},
                   /*expected_accounts_removed=*/{},
                   /*expected_accounts_in_storage=*/
                   {{main_path(), {"A"}}});
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

// Tests that a secondary profile gets deleted after its primary account is
// removed from the system.
// A secondary account of the deleted profile gets moved to the primary profile
// since it's an only remaining profile.
TEST_F(AccountProfileMapperTest, RemovePrimaryAccountFromSecondaryProfile) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  SetPrimaryAccountForProfile(other_path, "B");
  TestMapperUpdateGaia(mapper,
                       /*accounts_in_facade=*/{"A", "C"},
                       /*expected_accounts_upserted=*/{{main_path(), {"C"}}},
                       /*expected_accounts_removed=*/{{other_path, {"B"}}},
                       /*expected_accounts_in_storage=*/
                       {{main_path(), {"A", "C"}}});
  ProfileAttributesStorageTestObserver(attributes_storage())
      .WaitForProfileBeingDeleted(other_path);
}

// Tests that a secondary profile gets deleted after its primary account is
// removed from the system.
// A secondary account of the deleted profile stays unassigned since there are
// still several profiles.
TEST_F(AccountProfileMapperTest,
       RemovePrimaryAccountFromSecondaryProfile_MultipleProfiles) {
  base::FilePath second_path = GetProfilePath("Second");
  base::FilePath third_path = GetProfilePath("Third");
  AccountProfileMapper* mapper = CreateMapper(
      {{main_path(), {"A"}}, {second_path, {"B", "C"}}, {third_path, {"D"}}});
  SetPrimaryAccountForProfile(second_path, "B");
  TestMapperUpdateGaia(
      mapper,
      /*accounts_in_facade=*/{"A", "C", "D"},
      /*expected_accounts_upserted=*/{{base::FilePath(), {"C"}}},
      /*expected_accounts_removed=*/{{second_path, {"B"}}},
      /*expected_accounts_in_storage=*/
      {{main_path(), {"A"}}, {third_path, {"D"}}});
  ProfileAttributesStorageTestObserver(attributes_storage())
      .WaitForProfileBeingDeleted(second_path);
}

// Tests that a secondary profile gets deleted after its primary account was
// removed from the system before startup.
// A secondary account of the deleted profile gets moved to the primary profile
// since it's an only remaining profile.
TEST_F(AccountProfileMapperTest,
       RemovePrimaryAccountFromSecondaryProfile_AtInitialization) {
  base::FilePath other_path = GetProfilePath("Other");
  CreateMapperNonInitialized({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  SetPrimaryAccountForProfile(other_path, "B");
  CompleteFacadeGetAccountsGaia({"A", "C"});
  ExpectAccountsInStorage({{main_path(), {"A", "C"}}});
  ProfileAttributesStorageTestObserver(attributes_storage())
      .WaitForProfileBeingDeleted(other_path);
}

// Tests that a secondary profile doesn't get deleted after its secondary
// account is removed from the system.
TEST_F(AccountProfileMapperTest, RemoveSecondaryAccountFromSecondaryProfile) {
  base::FilePath other_path = GetProfilePath("Other");
  AccountProfileMapper* mapper =
      CreateMapper({{main_path(), {"A"}}, {other_path, {"B", "C"}}});
  SetPrimaryAccountForProfile(other_path, "B");
  TestMapperUpdateGaia(mapper,
                       /*accounts_in_facade=*/{"A", "B"},
                       /*expected_accounts_upserted=*/{},
                       /*expected_accounts_removed=*/{{other_path, {"C"}}},
                       /*expected_accounts_in_storage=*/
                       {{main_path(), {"A"}}, {other_path, {"B"}}});
}

// Tests that the primary profile doesn't get deleted even after its primary
// account is removed from the system.
TEST_F(AccountProfileMapperTest, RemovePrimaryAccountFromPrimaryProfile) {
  AccountProfileMapper* mapper = CreateMapper({{main_path(), {"A", "B"}}});
  SetPrimaryAccountForProfile(main_path(), "A");
  TestMapperUpdateGaia(mapper,
                       /*accounts_in_facade=*/{"B"},
                       /*expected_accounts_upserted=*/{},
                       /*expected_accounts_removed=*/{{main_path(), {"A"}}},
                       /*expected_accounts_in_storage=*/
                       {{main_path(), {"B"}}});
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
  absl::optional<AccountProfileMapper::AddAccountResult> result =
      AccountProfileMapper::AddAccountResult{other_path, account_c};
  AccountManagerFacade::AccountAdditionSource source =
      AccountManagerFacade::AccountAdditionSource::kOgbAddAccount;

  // Success: Add account to existing profile.
  ExpectFacadeShowAddAccountDialogCalled(source, account_c);
  EXPECT_CALL(account_added_callback, Run(AddAccountResultEqual(result)));
  EXPECT_CALL(mock_observer,
              OnAccountUpserted(other_path, AccountEqual(account_c)));
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);
  mapper->ShowAddAccountDialog(other_path, source,
                               account_added_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  ExpectAccountsInStorage({{main_path(), {"A"}}, {other_path, {"B", "C"}}});

  // Success: Add account to existing profile (with no callback provided).
  Account account_d = AccountFromGaiaID("D");
  ExpectFacadeShowAddAccountDialogCalled(source, account_d);
  EXPECT_CALL(mock_observer,
              OnAccountUpserted(other_path, AccountEqual(account_d)));
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);
  mapper->ShowAddAccountDialog(other_path, source,
                               AccountProfileMapper::AddAccountCallback());
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  ExpectAccountsInStorage(
      {{main_path(), {"A"}}, {other_path, {"B", "C", "D"}}});

  // Failure: Add account that already exists.
  ExpectFacadeShowAddAccountDialogCalled(source, account_c);
  EXPECT_CALL(account_added_callback, Run(testing::Eq(absl::nullopt)));
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
  result = {base::FilePath(), account_e};
  EXPECT_CALL(account_added_callback, Run(AddAccountResultEqual(result)));
  EXPECT_CALL(mock_observer,
              OnAccountUpserted(base::FilePath(), AccountEqual(account_e)));
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);
  mapper->ShowAddAccountDialog(unknown_path, source,
                               account_added_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Failure: Non-Gaia account.
  ExpectFacadeShowAddAccountDialogCalled(source, NonGaiaAccountFromID("E"));
  EXPECT_CALL(account_added_callback, Run(testing::Eq(absl::nullopt)));
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);
  mapper->ShowAddAccountDialog(other_path, source,
                               account_added_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Failure: Flow aborted.
  ExpectFacadeShowAddAccountDialogCalled(source, absl::nullopt);
  EXPECT_CALL(account_added_callback, Run(testing::Eq(absl::nullopt)));
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_, testing::_)).Times(0);
  mapper->ShowAddAccountDialog(other_path, source,
                               account_added_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // No account was assigned by any the failures above.
  ExpectAccountsInStorage(
      {{main_path(), {"A"}}, {other_path, {"B", "C", "D"}}});
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
  base::FilePath new_profile_path = GetProfilePath("Profile 1");
  absl::optional<AccountProfileMapper::AddAccountResult> result =
      AccountProfileMapper::AddAccountResult{new_profile_path, account_b};
  EXPECT_CALL(account_added_callback, Run(AddAccountResultEqual(result)));
  EXPECT_CALL(mock_observer,
              OnAccountUpserted(new_profile_path, AccountEqual(account_b)));
  ProfileWaiter profile_waiter;

  // Create the profile.
  mapper->ShowAddAccountDialogAndCreateNewProfile(source,
                                                  account_added_callback.Get());
  Profile* new_profile = profile_waiter.WaitForProfileAdded();

  // Check that the profile was created and configured.
  testing::Mock::VerifyAndClearExpectations(&account_added_callback);
  testing::Mock::VerifyAndClearExpectations(mock_facade());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_EQ(new_profile->GetPath(), new_profile_path);
  ExpectAccountsInStorage({{main_path(), {"A"}}, {new_profile_path, {"B"}}});
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
  absl::optional<AccountProfileMapper::AddAccountResult> result =
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
  ExpectAccountsInStorage({{main_path(), {"A", "C"}}, {other_path, {"B"}}});

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

  ExpectAccountsInStorage({{main_path(), {"A", "C"}}, {other_path, {"B"}}});
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
  EXPECT_CALL(account_added_callback, Run(testing::Eq(absl::nullopt)));
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
  ExpectAccountsInStorage({{main_path(), {"A"}}});
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
  absl::optional<AccountProfileMapper::AddAccountResult> result =
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
  ExpectAccountsInStorage(
      {{main_path(), {"A"}}, {other_path, {"B"}}, {new_profile_path, {"C"}}});
  ProfileAttributesEntry* entry =
      attributes_storage()->GetProfileAttributesWithPath(new_profile_path);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->IsOmitted());
  EXPECT_TRUE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetSigninState(), SigninState::kNotSignedIn);
}
