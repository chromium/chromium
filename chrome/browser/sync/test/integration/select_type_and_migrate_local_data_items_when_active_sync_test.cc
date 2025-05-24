// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/contact_info_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "components/sync/test/nigori_test_utils.h"
#include "components/sync_bookmarks/switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using autofill::AutofillProfile;
using bookmarks::BookmarkNode;
using contact_info_helper::AddressDataManagerProfileChecker;
using passwords_helper::CreateTestPasswordForm;
using testing::UnorderedElementsAre;

using password_manager::PasswordForm;

class MockAddressDataManagerObserver
    : public autofill::AddressDataManager::Observer {
 public:
  MOCK_METHOD(void, OnAddressDataChanged, (), (override));
};

class SelectTypeAndMigrateLocalDataItemsWhenActiveTest : public SyncTest {
 public:
  SelectTypeAndMigrateLocalDataItemsWhenActiveTest()
      : SyncTest(SINGLE_CLIENT),
        password_(CreateTestPasswordForm(0)) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {switches::kSyncEnableBookmarksInTransportMode,
         autofill::features::kAutofillSupportLastNamePrefix},
        /*disabled_features=*/{
            syncer::kSyncEnableContactInfoDataTypeForCustomPassphraseUsers});

    // Ensure profile creation occurs after flag initialization to guarantee
    // their effectiveness within the profile's constructor.
    address_ =
        std::make_unique<AutofillProfile>(autofill::test::GetFullProfile());
  }

  // In SINGLE_CLIENT tests, there's only a single PersonalDataManager.
  autofill::PersonalDataManager* GetPersonalDataManager() const {
    return contact_info_helper::GetPersonalDataManager(GetProfile(0));
  }

  const AutofillProfile& address() { return *address_; }
  const PasswordForm& password() { return password_; }

  // Sign in with `signin::ConsentLevel::kSignin`.
  void SignIn() {
    ASSERT_TRUE(
        GetClient(0)->SignInPrimaryAccount(signin::ConsentLevel::kSignin));
    // Enable account storage for bookmarks.
    SigninPrefs(*GetProfile(0)->GetPrefs())
        .SetBookmarksExplicitBrowserSignin(
            GetSyncService(0)->GetSyncAccountInfoForPrefs().gaia, true);
    ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  }

  void SaveLocalAddress() {
    GetPersonalDataManager()->address_data_manager().AddProfile(*address_);
    EXPECT_TRUE(AddressDataManagerProfileChecker(
                    &GetPersonalDataManager()->address_data_manager(),
                    UnorderedElementsAre(*address_))
                    .Wait());
  }

  const BookmarkNode* SaveLocalBookmark() {
    return bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                                    /*index=*/0, u"Local",
                                    GURL("http://local.com/"));
  }

  std::vector<const AutofillProfile*> GetLocalAddresses() {
    return GetPersonalDataManager()
        ->address_data_manager()
        .GetProfilesByRecordType(
            autofill::AutofillProfile::RecordType::kLocalOrSyncable);
  }

  std::vector<std::unique_ptr<password_manager::PasswordForm>>
  GetLocalPasswords() {
    return passwords_helper::GetAllLogins(
        passwords_helper::GetProfilePasswordStoreInterface(0));
  }

  bookmarks::BookmarkModel* bookmark_model() {
    return bookmarks_helper::GetBookmarkModel(0);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<AutofillProfile> address_;
  PasswordForm password_;
};

IN_PROC_BROWSER_TEST_F(SelectTypeAndMigrateLocalDataItemsWhenActiveTest,
                       ShouldSelectTypeEvenIfPreviouslyDeselected) {
  ASSERT_TRUE(SetupClients());

  SignIn();
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kAutofill, false);
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kBookmarks, false);

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::BOOKMARKS));

  // This should turn on account storage for the respective types.
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::PASSWORDS, {});
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::CONTACT_INFO, {});
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::BOOKMARKS, {});

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::BOOKMARKS));
}

IN_PROC_BROWSER_TEST_F(SelectTypeAndMigrateLocalDataItemsWhenActiveTest,
                       ShouldUploadAddress) {
  ASSERT_TRUE(SetupClients());

  SaveLocalAddress();
  ASSERT_EQ(1u, GetLocalAddresses().size());

  SignIn();
  ASSERT_EQ(
      0u, fake_server_->GetSyncEntitiesByDataType(syncer::CONTACT_INFO).size());

  // This should migrate the address.
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::CONTACT_INFO, {address().guid()});

  EXPECT_TRUE(ServerCountMatchStatusChecker(syncer::CONTACT_INFO, 1).Wait());
  EXPECT_EQ(0u, GetLocalAddresses().size());
}

IN_PROC_BROWSER_TEST_F(SelectTypeAndMigrateLocalDataItemsWhenActiveTest,
                       ShouldUploadPassword) {
  ASSERT_TRUE(SetupClients());

  // Set up a locally saved password.
  passwords_helper::GetProfilePasswordStoreInterface(0)->AddLogin(password());
  ASSERT_EQ(1u, GetLocalPasswords().size());

  SignIn();
  ASSERT_EQ(0u,
            fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS).size());

  // This should migrate the password.
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::PASSWORDS, {PasswordFormUniqueKey(password())});

  EXPECT_TRUE(ServerCountMatchStatusChecker(syncer::PASSWORDS, 1).Wait());
  EXPECT_EQ(0u, GetLocalPasswords().size());
}

IN_PROC_BROWSER_TEST_F(SelectTypeAndMigrateLocalDataItemsWhenActiveTest,
                       ShouldUploadBookmark) {
  ASSERT_TRUE(SetupClients());

  const BookmarkNode* bookmark = SaveLocalBookmark();
  ASSERT_EQ(1u, bookmark_model()->bookmark_bar_node()->children().size());

  SignIn();
  ASSERT_EQ(0u,
            fake_server_->GetSyncEntitiesByDataType(syncer::BOOKMARKS).size());

  // This should migrate the bookmark.
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::BOOKMARKS, {bookmark->id()});

  EXPECT_TRUE(ServerCountMatchStatusChecker(syncer::BOOKMARKS, 1).Wait());
  EXPECT_EQ(0u, bookmark_model()->bookmark_bar_node()->children().size());
  EXPECT_EQ(1u,
            bookmark_model()->account_bookmark_bar_node()->children().size());
}

IN_PROC_BROWSER_TEST_F(SelectTypeAndMigrateLocalDataItemsWhenActiveTest,
                       ShouldUploadMultiplePasswords) {
  ASSERT_TRUE(SetupClients());

  // Set up two locally saved passwords.
  PasswordForm second_password = CreateTestPasswordForm(1);
  passwords_helper::GetProfilePasswordStoreInterface(0)->AddLogin(password());
  passwords_helper::GetProfilePasswordStoreInterface(0)->AddLogin(
      second_password);
  ASSERT_EQ(2u, GetLocalPasswords().size());

  SignIn();
  ASSERT_EQ(0u,
            fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS).size());

  // This should migrate the passwords.
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::PASSWORDS, {PasswordFormUniqueKey(password()),
                          PasswordFormUniqueKey(second_password)});

  EXPECT_TRUE(ServerCountMatchStatusChecker(syncer::PASSWORDS, 2).Wait());
  EXPECT_EQ(0u, GetLocalPasswords().size());
}

// Remove this test and replace it with the one in
// `SelectTypeAndMigrateLocalDataItemsWhenActiveWithContactInfoForCustomPassphraseUsersTest`
// once `kSyncEnableContactInfoDataTypeForCustomPassphraseUsers` is enabled by
// default.
IN_PROC_BROWSER_TEST_F(SelectTypeAndMigrateLocalDataItemsWhenActiveTest,
                       ShouldNotUploadAddressWithCustomPassphrase) {
  ASSERT_TRUE(SetupClients());

  // Set up a custom passphrase.
  const syncer::KeyParamsForTesting kCustomPassphraseKeyParams =
      syncer::Pbkdf2PassphraseKeyParamsForTesting("hritika");
  SetNigoriInFakeServer(
      BuildCustomPassphraseNigoriSpecifics(kCustomPassphraseKeyParams),
      GetFakeServer());

  SaveLocalAddress();
  ASSERT_EQ(1u, GetLocalAddresses().size());

  SignIn();
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());
  ASSERT_EQ(
      0u, fake_server_->GetSyncEntitiesByDataType(syncer::CONTACT_INFO).size());

  // This should not turn on account storage. The address will stay local.
  // Entering the passphrase does not change that.
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::CONTACT_INFO, {address().guid()});
  EXPECT_EQ(1u,
            GetSyncService(0)->GetQueuedLocalDataMigrationItemCountForTest());
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      kCustomPassphraseKeyParams.password));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));

  EXPECT_EQ(
      0u, fake_server_->GetSyncEntitiesByDataType(syncer::CONTACT_INFO).size());
  EXPECT_EQ(1u, GetLocalAddresses().size());

  // The address is still in the queue, but will be cleared once the user signs
  // out.
  EXPECT_EQ(1u,
            GetSyncService(0)->GetQueuedLocalDataMigrationItemCountForTest());
  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_EQ(0u,
            GetSyncService(0)->GetQueuedLocalDataMigrationItemCountForTest());
}

IN_PROC_BROWSER_TEST_F(SelectTypeAndMigrateLocalDataItemsWhenActiveTest,
                       ShouldUploadPasswordWithCustomPassphrase) {
  ASSERT_TRUE(SetupClients());

  // Set up a custom passphrase.
  const syncer::KeyParamsForTesting kCustomPassphraseKeyParams =
      syncer::Pbkdf2PassphraseKeyParamsForTesting("hritika");
  SetNigoriInFakeServer(
      BuildCustomPassphraseNigoriSpecifics(kCustomPassphraseKeyParams),
      GetFakeServer());

  // Set up a locally saved password.
  passwords_helper::GetProfilePasswordStoreInterface(0)->AddLogin(password());
  ASSERT_EQ(1u, GetLocalPasswords().size());

  SignIn();
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());
  ASSERT_EQ(0u,
            fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS).size());

  // This should not activate account storage yet. The passphrase error has to
  // be resolved first.
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::PASSWORDS, {PasswordFormUniqueKey(password())});
  EXPECT_EQ(1u,
            GetSyncService(0)->GetQueuedLocalDataMigrationItemCountForTest());

  // Enter the passphrase.
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      kCustomPassphraseKeyParams.password));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());

  EXPECT_TRUE(ServerCountMatchStatusChecker(syncer::PASSWORDS, 1).Wait());
  EXPECT_EQ(0u, GetLocalPasswords().size());
}

IN_PROC_BROWSER_TEST_F(SelectTypeAndMigrateLocalDataItemsWhenActiveTest,
                       ShouldNotUploadPasswordWithSyncDisabled) {
  ASSERT_TRUE(SetupClients());

  // Use a custom passphrase in order to simulate waiting for the passphrase to
  // be entered. During that time, sign out to disable sync.
  const syncer::KeyParamsForTesting kCustomPassphraseKeyParams =
      syncer::Pbkdf2PassphraseKeyParamsForTesting("hritika");
  SetNigoriInFakeServer(
      BuildCustomPassphraseNigoriSpecifics(kCustomPassphraseKeyParams),
      GetFakeServer());

  // Set up a locally saved password.
  passwords_helper::GetProfilePasswordStoreInterface(0)->AddLogin(password());
  ASSERT_EQ(1u, GetLocalPasswords().size());

  SignIn();
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());
  ASSERT_EQ(0u,
            fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS).size());

  // This should not activate account storage yet. The passphrase error has to
  // be resolved first.
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::PASSWORDS, {PasswordFormUniqueKey(password())});
  EXPECT_EQ(1u,
            GetSyncService(0)->GetQueuedLocalDataMigrationItemCountForTest());

  // Sign out once, then sign in again. Account storage was enabled but the
  // password is not migrated anymore.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_EQ(GetSyncService(0)->GetTransportState(),
            syncer::SyncService::TransportState::DISABLED);
  SignIn();
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      kCustomPassphraseKeyParams.password));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  EXPECT_EQ(0u,
            fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS).size());
  EXPECT_EQ(1u, GetLocalPasswords().size());
  EXPECT_EQ(0u,
            GetSyncService(0)->GetQueuedLocalDataMigrationItemCountForTest());
}

class SelectTypeAndMigrateLocalDataItemsWhenActiveTestWithPolicy
    : public SelectTypeAndMigrateLocalDataItemsWhenActiveTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    SelectTypeAndMigrateLocalDataItemsWhenActiveTest::
        SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider>*
  policy_provider() {
    return &policy_provider_;
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(
    SelectTypeAndMigrateLocalDataItemsWhenActiveTestWithPolicy,
    ShouldNotUploadAddressWithPolicy) {
  ASSERT_TRUE(SetupClients());

  SaveLocalAddress();
  ASSERT_EQ(1u, GetLocalAddresses().size());

  // Disable addresses via the kSyncTypesListDisabled policy.
  base::Value::List disabled_types;
  disabled_types.Append("autofill");
  policy::PolicyMap policies;
  policies.Set(policy::key::kSyncTypesListDisabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(disabled_types)), nullptr);
  policy_provider()->UpdateChromePolicy(policies);

  SignIn();
  ASSERT_EQ(
      0u, fake_server_->GetSyncEntitiesByDataType(syncer::CONTACT_INFO).size());

  // This should not turn on account storage. The address will stay local.
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::CONTACT_INFO, {address().guid()});
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));

  EXPECT_EQ(
      0u, fake_server_->GetSyncEntitiesByDataType(syncer::CONTACT_INFO).size());
  EXPECT_EQ(1u, GetLocalAddresses().size());
  EXPECT_EQ(0u,
            GetSyncService(0)->GetQueuedLocalDataMigrationItemCountForTest());
}

IN_PROC_BROWSER_TEST_F(
    SelectTypeAndMigrateLocalDataItemsWhenActiveTestWithPolicy,
    ShouldNotUploadPasswordWithPolicy) {
  ASSERT_TRUE(SetupClients());

  // Set up a locally saved password.
  passwords_helper::GetProfilePasswordStoreInterface(0)->AddLogin(password());
  ASSERT_EQ(1u, GetLocalPasswords().size());

  // Disable passwords via the kSyncTypesListDisabled policy.
  base::Value::List disabled_types;
  disabled_types.Append("passwords");
  policy::PolicyMap policies;
  policies.Set(policy::key::kSyncTypesListDisabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(disabled_types)), nullptr);
  policy_provider()->UpdateChromePolicy(policies);

  SignIn();

  // This should not turn on account storage. The password will stay local.
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::PASSWORDS, {PasswordFormUniqueKey(password())});
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  EXPECT_EQ(0u,
            fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS).size());
  EXPECT_EQ(1u, GetLocalPasswords().size());
  EXPECT_EQ(0u,
            GetSyncService(0)->GetQueuedLocalDataMigrationItemCountForTest());
}

class
    SelectTypeAndMigrateLocalDataItemsWhenActiveWithContactInfoForCustomPassphraseUsersTest
    : public SelectTypeAndMigrateLocalDataItemsWhenActiveTest {
 public:
  SelectTypeAndMigrateLocalDataItemsWhenActiveWithContactInfoForCustomPassphraseUsersTest() =
      default;

 private:
  base::test::ScopedFeatureList feature_list_{
      syncer::kSyncEnableContactInfoDataTypeForCustomPassphraseUsers};
};

IN_PROC_BROWSER_TEST_F(
    SelectTypeAndMigrateLocalDataItemsWhenActiveWithContactInfoForCustomPassphraseUsersTest,
    ShouldUploadAddressWithCustomPassphrase) {
  ASSERT_TRUE(SetupClients());

  // Set up a custom passphrase.
  const syncer::KeyParamsForTesting kCustomPassphraseKeyParams =
      syncer::Pbkdf2PassphraseKeyParamsForTesting("hritika");
  SetNigoriInFakeServer(
      BuildCustomPassphraseNigoriSpecifics(kCustomPassphraseKeyParams),
      GetFakeServer());

  SaveLocalAddress();
  ASSERT_EQ(1u, GetLocalAddresses().size());

  SignIn();
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());
  ASSERT_EQ(
      0u, fake_server_->GetSyncEntitiesByDataType(syncer::CONTACT_INFO).size());

  // This should migrate the address.
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::CONTACT_INFO, {address().guid()});

  EXPECT_TRUE(ServerCountMatchStatusChecker(syncer::CONTACT_INFO, 1).Wait());
  EXPECT_EQ(0u, GetLocalAddresses().size());
}

// Overwrite the Sync test account with a non-gmail account. This treats it as
// a Dasher account.
class SelectTypeAndMigrateLocalDataItemsWhenActiveWithManagedAccountTest
    : public SelectTypeAndMigrateLocalDataItemsWhenActiveTest {
 public:
  SelectTypeAndMigrateLocalDataItemsWhenActiveWithManagedAccountTest() {
    // This can't be done in `SetUpCommandLine()` because `SyncTest::SetUp()`
    // already consumes the parameter.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSyncUserForTest, "user@managed-domain.com");
  }

  void SignIn(const std::string& hosted_domain) {
    ASSERT_TRUE(
        GetClient(0)->SignInPrimaryAccount(signin::ConsentLevel::kSignin));

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(GetProfile(0));
    CoreAccountInfo account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

    signin::SimulateSuccessfulFetchOfAccountInfo(
        identity_manager, account.account_id, account.email, account.gaia,
        hosted_domain, "Full Name", "Given Name", "en-US", "");

    ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  }
};

IN_PROC_BROWSER_TEST_F(
    SelectTypeAndMigrateLocalDataItemsWhenActiveWithManagedAccountTest,
    ShouldNotUploadAddressWithManagedAccount) {
  ASSERT_TRUE(SetupClients());

  SaveLocalAddress();
  ASSERT_EQ(1u, GetLocalAddresses().size());

  // Sign in with a managed account.
  SignIn(/*hosted_domain=*/"managed-domain.com");

  // This should not turn on account storage. The address will stay local.
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::CONTACT_INFO, {address().guid()});

  EXPECT_EQ(
      0u, fake_server_->GetSyncEntitiesByDataType(syncer::CONTACT_INFO).size());
  EXPECT_EQ(1u, GetLocalAddresses().size());

  // The address is still in the queue, but will be cleared once the user signs
  // out.
  EXPECT_EQ(1u,
            GetSyncService(0)->GetQueuedLocalDataMigrationItemCountForTest());
  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_EQ(0u,
            GetSyncService(0)->GetQueuedLocalDataMigrationItemCountForTest());
}

// Overwrite the Sync test account with an @google.com managed account.
class SelectTypeAndMigrateLocalDataItemsWhenActiveWithGoogleManagedAccountTest
    : public SelectTypeAndMigrateLocalDataItemsWhenActiveWithManagedAccountTest {
 public:
  SelectTypeAndMigrateLocalDataItemsWhenActiveWithGoogleManagedAccountTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSyncUserForTest, "user@google.com");
  }
};

IN_PROC_BROWSER_TEST_F(
    SelectTypeAndMigrateLocalDataItemsWhenActiveWithGoogleManagedAccountTest,
    ShouldUploadAddressWithGoogleManagedAccount) {
  ASSERT_TRUE(SetupClients());

  SaveLocalAddress();
  ASSERT_EQ(1u, GetLocalAddresses().size());

  // Sign in with a Google managed account.
  SignIn(/*hosted_domain=*/"google.com");
  ASSERT_EQ(
      0u, fake_server_->GetSyncEntitiesByDataType(syncer::CONTACT_INFO).size());

  // This should migrate the address.
  GetSyncService(0)->SelectTypeAndMigrateLocalDataItemsWhenActive(
      syncer::CONTACT_INFO, {address().guid()});

  EXPECT_TRUE(ServerCountMatchStatusChecker(syncer::CONTACT_INFO, 1).Wait());
  EXPECT_EQ(0u, GetLocalAddresses().size());
  EXPECT_EQ(0u,
            GetSyncService(0)->GetQueuedLocalDataMigrationItemCountForTest());
}

}  // namespace
