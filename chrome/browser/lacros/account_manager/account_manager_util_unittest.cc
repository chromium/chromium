// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/account_manager_util.h"

#include <algorithm>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using account_manager::Account;
using account_manager::AccountKey;
using account_manager::AccountManagerFacade;
using testing::Field;
using testing::UnorderedElementsAre;

namespace {

constexpr account_manager::AccountType kGaiaType =
    account_manager::AccountType::kGaia;

// Map from profile path to a vector of GaiaIds.
using AccountMapping =
    base::flat_map<base::FilePath, base::flat_set<std::string>>;

// Synthetizes a `Account` from a Gaia ID, with a dummy email.
Account AccountFromGaiaID(const std::string& gaia_id) {
  AccountKey key(gaia_id, kGaiaType);
  return {key, gaia_id + std::string("@gmail.com")};
}

}  // namespace

class AccountManagerUtilTest : public testing::Test {
 public:
  AccountManagerUtilTest()
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

  ~AccountManagerUtilTest() override = default;

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

  std::unique_ptr<AccountProfileMapper> CreateMapper(
      const AccountMapping& accounts) {
    // Mapper asks the facade for accounts upon construction.
    std::vector<Account> accounts_in_facade;
    for (const auto& path_accounts_pair : accounts) {
      for (const std::string& id : path_accounts_pair.second) {
        accounts_in_facade.push_back(AccountFromGaiaID(id));
      }
    }
    ON_CALL(mock_facade_, GetAccounts(testing::_))
        .WillByDefault(
            [&accounts_in_facade](
                base::OnceCallback<void(const std::vector<Account>&)>
                    callback) { std::move(callback).Run(accounts_in_facade); });
    auto mapper = std::make_unique<AccountProfileMapper>(
        mock_facade(), attributes_storage(),
        testing_profile_manager_.local_state()->Get());
    SetAccountsInStorage(accounts);
    return mapper;
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
  content::BrowserTaskEnvironment task_environment_;
  testing::NiceMock<account_manager::MockAccountManagerFacade> mock_facade_;
  TestingProfileManager testing_profile_manager_;
  base::FilePath main_path_;
};

TEST_F(AccountManagerUtilTest, GetAllAvailableAccounts) {
  base::FilePath second_path = GetProfilePath("Second");
  base::FilePath third_path = GetProfilePath("Third");
  base::FilePath unassigned = base::FilePath();
  std::unique_ptr<AccountProfileMapper> mapper =
      CreateMapper({{main_path(), {"A"}},
                    {second_path, {"B", "C"}},
                    {third_path, {"D"}},
                    {unassigned, {"E"}}});

  base::MockRepeatingCallback<void(const std::vector<Account>&)> mock_callback;

  // Accounts from all other profiles are returned, incl. unassigned.
  EXPECT_CALL(mock_callback,
              Run(testing::UnorderedElementsAre(
                  Field(&Account::key, AccountKey{"B", kGaiaType}),
                  Field(&Account::key, AccountKey{"C", kGaiaType}),
                  Field(&Account::key, AccountKey{"D", kGaiaType}),
                  Field(&Account::key, AccountKey{"E", kGaiaType}))));
  GetAllAvailableAccounts(mapper.get(), main_path(), mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback,
              Run(testing::UnorderedElementsAre(
                  Field(&Account::key, AccountKey{"A", kGaiaType}),
                  Field(&Account::key, AccountKey{"D", kGaiaType}),
                  Field(&Account::key, AccountKey{"E", kGaiaType}))));
  GetAllAvailableAccounts(mapper.get(), second_path, mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Syncing status does not change anything here.
  SetPrimaryAccountForProfile(main_path(), "A");
  SetPrimaryAccountForProfile(second_path, "B");
  EXPECT_CALL(mock_callback,
              Run(testing::UnorderedElementsAre(
                  Field(&Account::key, AccountKey{"B", kGaiaType}),
                  Field(&Account::key, AccountKey{"C", kGaiaType}),
                  Field(&Account::key, AccountKey{"D", kGaiaType}),
                  Field(&Account::key, AccountKey{"E", kGaiaType}))));
  GetAllAvailableAccounts(mapper.get(), main_path(), mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback,
              Run(testing::UnorderedElementsAre(
                  Field(&Account::key, AccountKey{"A", kGaiaType}),
                  Field(&Account::key, AccountKey{"D", kGaiaType}),
                  Field(&Account::key, AccountKey{"E", kGaiaType}))));
  GetAllAvailableAccounts(mapper.get(), second_path, mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Non existing profile path or empty profile path returns all accounts.
  base::FilePath non_existing_path = base::FilePath("Foo");
  EXPECT_CALL(mock_callback,
              Run(testing::UnorderedElementsAre(
                  Field(&Account::key, AccountKey{"A", kGaiaType}),
                  Field(&Account::key, AccountKey{"B", kGaiaType}),
                  Field(&Account::key, AccountKey{"C", kGaiaType}),
                  Field(&Account::key, AccountKey{"D", kGaiaType}),
                  Field(&Account::key, AccountKey{"E", kGaiaType}))))
      .Times(2);
  GetAllAvailableAccounts(mapper.get(), non_existing_path, mock_callback.Get());
  GetAllAvailableAccounts(mapper.get(), base::FilePath(), mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);
}

TEST_F(AccountManagerUtilTest, GetAccountsAvailableAsSecondary_Overlapping) {
  base::FilePath second_path = GetProfilePath("Second");
  base::FilePath unassigned = base::FilePath();
  std::unique_ptr<AccountProfileMapper> mapper =
      CreateMapper({{main_path(), {"A", "B"}},
                    {second_path, {"B", "C"}},
                    {unassigned, {"E"}}});

  base::MockRepeatingCallback<void(const std::vector<Account>&)> mock_callback;

  // All accounts not in the current profile are returned, incl. unassigned.
  EXPECT_CALL(mock_callback,
              Run(testing::UnorderedElementsAre(
                  Field(&Account::key, AccountKey{"C", kGaiaType}),
                  Field(&Account::key, AccountKey{"E", kGaiaType}))));
  GetAllAvailableAccounts(mapper.get(), main_path(), mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback,
              Run(testing::UnorderedElementsAre(
                  Field(&Account::key, AccountKey{"A", kGaiaType}),
                  Field(&Account::key, AccountKey{"E", kGaiaType}))));
  GetAllAvailableAccounts(mapper.get(), second_path, mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Syncing status does not change anything here.
  SetPrimaryAccountForProfile(main_path(), "A");
  SetPrimaryAccountForProfile(second_path, "B");
  EXPECT_CALL(mock_callback,
              Run(testing::UnorderedElementsAre(
                  Field(&Account::key, AccountKey{"C", kGaiaType}),
                  Field(&Account::key, AccountKey{"E", kGaiaType}))));
  GetAllAvailableAccounts(mapper.get(), main_path(), mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback,
              Run(testing::UnorderedElementsAre(
                  Field(&Account::key, AccountKey{"A", kGaiaType}),
                  Field(&Account::key, AccountKey{"E", kGaiaType}))));
  GetAllAvailableAccounts(mapper.get(), second_path, mock_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_callback);
}
