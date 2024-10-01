// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/contact_info_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/addresses/contact_info_sync_util.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/contact_info_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#endif

namespace {

using autofill::AutofillProfile;
using contact_info_helper::AddressDataManagerProfileChecker;
using contact_info_helper::BuildTestAccountProfile;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

#if !BUILDFLAG(IS_ANDROID)
std::string CreateSerializedProtoField(int field_number,
                                       const std::string& value) {
  std::string result;
  google::protobuf::io::StringOutputStream string_stream(&result);
  google::protobuf::io::CodedOutputStream output(&string_stream);
  google::protobuf::internal::WireFormatLite::WriteTag(
      field_number,
      google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED,
      &output);
  output.WriteVarint32(value.size());
  output.WriteString(value);
  return result;
}

// Matches a sync::entity_data has a contact info field with `guid` and a set of
// `unknown_fields`.
MATCHER_P2(HasContactInfoWithGuidAndUnknownFields, guid, unknown_fields, "") {
  return arg.specifics().contact_info().guid() == guid &&
         arg.specifics().contact_info().unknown_fields() == unknown_fields;
}
#endif

// Checker to wait until the CONTACT_INFO datatype becomes (in)active, depending
// on `expect_active`.
// This is required because ContactInfoDataTypeController has custom logic to
// wait, and stays temporarily stopped even after sync-the-transport is active,
// until account capabilities are determined for eligibility.
class ContactInfoActiveChecker : public SingleClientStatusChangeChecker {
 public:
  ContactInfoActiveChecker(syncer::SyncServiceImpl* service, bool expect_active)
      : SingleClientStatusChangeChecker(service),
        expect_active_(expect_active) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    return service()->GetActiveDataTypes().Has(syncer::CONTACT_INFO) ==
           expect_active_;
  }

 private:
  const bool expect_active_;
};

// Helper class to wait until the fake server's ContactInfoSpecifics match a
// given predicate.
// Unfortunately, since protos don't have an equality operator, the comparisons
// are based on the `SerializeAsString()` representation of the specifics.
class FakeServerSpecificsChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher = testing::Matcher<std::vector<std::string>>;

  explicit FakeServerSpecificsChecker(const Matcher& matcher)
      : matcher_(matcher) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    std::vector<std::string> specifics;
    for (const sync_pb::SyncEntity& entity :
         fake_server()->GetSyncEntitiesByDataType(syncer::CONTACT_INFO)) {
      specifics.push_back(
          entity.specifics().contact_info().SerializeAsString());
    }
    testing::StringMatchResultListener listener;
    bool matches = testing::ExplainMatchResult(matcher_, specifics, &listener);
    *os << listener.str();
    return matches;
  }

 private:
  const Matcher matcher_;
};

// Since the sync server operates in terms of entity specifics, this helper
// function converts a given `profile` to the equivalent ContactInfoSpecifics.
sync_pb::ContactInfoSpecifics AsContactInfoSpecifics(
    const AutofillProfile& profile) {
  return autofill::CreateContactInfoEntityDataFromAutofillProfile(
             profile, /*base_contact_info_specifics=*/{})
      ->specifics.contact_info();
}

// Adds the given `specifics` to the `fake_server` at creation time 0.
void AddSpecificsToServer(const sync_pb::ContactInfoSpecifics& specifics,
                          fake_server::FakeServer* fake_server) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_contact_info()->CopyFrom(specifics);
  fake_server->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"profile", /*client_tag=*/specifics.guid(),
          /*entity_specifics=*/entity_specifics, /*creation_time=*/0,
          /*last_modified_time=*/0));
}

class SingleClientContactInfoSyncTest : public SyncTest {
 public:
  SingleClientContactInfoSyncTest() : SyncTest(SINGLE_CLIENT) {}

  // In SINGLE_CLIENT tests, there's only a single PersonalDataManager.
  autofill::PersonalDataManager* GetPersonalDataManager() const {
    return contact_info_helper::GetPersonalDataManager(GetProfile(0));
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientContactInfoSyncTest, DownloadInitialData) {
  const AutofillProfile kProfile = BuildTestAccountProfile();
  AddSpecificsToServer(AsContactInfoSpecifics(kProfile), GetFakeServer());
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(),
                  UnorderedElementsAre(kProfile))
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientContactInfoSyncTest, UploadProfile) {
  const AutofillProfile kProfile = BuildTestAccountProfile();
  ASSERT_TRUE(SetupSync());
  GetPersonalDataManager()->address_data_manager().AddProfile(kProfile);
  EXPECT_TRUE(FakeServerSpecificsChecker(
                  UnorderedElementsAre(
                      AsContactInfoSpecifics(kProfile).SerializeAsString()))
                  .Wait());
}

// Tests that profile changes due to `AutofillProfile::FinalizeAfterImport()`
// don't cause a reupload and hence can't cause ping-pong loops.
// This is not expected to happen because only the PersonalDataManager can
// trigger reuploads - and it only operates on finalized profiles.
IN_PROC_BROWSER_TEST_F(SingleClientContactInfoSyncTest, FinalizeAfterImport) {
  AutofillProfile unfinalized_profile(
      AutofillProfile::RecordType::kAccount,
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  unfinalized_profile.SetRawInfo(autofill::NAME_FULL, u"Full Name");
  AutofillProfile finalized_profile = unfinalized_profile;
  finalized_profile.FinalizeAfterImport();
  ASSERT_NE(unfinalized_profile, finalized_profile);

  // Add the `unfinalized_profile` to the server. An unfinalized profile is
  // never uploaded through Autofill, but non-Autofill clients might do so.
  AddSpecificsToServer(AsContactInfoSpecifics(unfinalized_profile),
                       GetFakeServer());
  ASSERT_TRUE(SetupSync());
  // Expect that the PersonalDataManager receives the `finalized_profile`. The
  // finalization step happen when reading the profile from AutofillTable.
  EXPECT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(),
                  UnorderedElementsAre(finalized_profile))
                  .Wait());

  // Expect that the finalized profile is not propagated back to the server.
  // Since the PersonalDatamanager is operating on a single thread, this is
  // verified by adding a dummy profile. It will only reach the server after any
  // already pending changes.
  const AutofillProfile kDummyProfile = BuildTestAccountProfile();
  GetPersonalDataManager()->address_data_manager().AddProfile(kDummyProfile);
  EXPECT_TRUE(
      FakeServerSpecificsChecker(
          UnorderedElementsAre(
              AsContactInfoSpecifics(unfinalized_profile).SerializeAsString(),
              AsContactInfoSpecifics(kDummyProfile).SerializeAsString()))
          .Wait());
}

// ChromeOS does not support signing out of a primary account.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientContactInfoSyncTest, ClearOnSignout) {
  const AutofillProfile kProfile = BuildTestAccountProfile();
  AddSpecificsToServer(AsContactInfoSpecifics(kProfile), GetFakeServer());
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(),
                  UnorderedElementsAre(kProfile))
                  .Wait());
  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(), IsEmpty())
                  .Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Specialized fixture to test the behavior for custom passphrase users with and
// without kSyncEnableContactInfoDataTypeForCustomPassphraseUsers enabled.
class SingleClientContactInfoPassphraseSyncTest
    : public SingleClientContactInfoSyncTest,
      public testing::WithParamInterface<bool> {
 public:
  SingleClientContactInfoPassphraseSyncTest() {
    passphrase_feature_.InitWithFeatureState(
        syncer::kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
        EnabledForPassphraseUsersTestParam());
  }

  bool EnabledForPassphraseUsersTestParam() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList passphrase_feature_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientContactInfoPassphraseSyncTest,
                         testing::Bool());

// TODO(336993637): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Passphrase DISABLED_Passphrase
#else
#define MAYBE_Passphrase Passphrase
#endif
IN_PROC_BROWSER_TEST_P(SingleClientContactInfoPassphraseSyncTest,
                       MAYBE_Passphrase) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase("123456");
  ASSERT_TRUE(
      ServerPassphraseTypeChecker(syncer::PassphraseType::kCustomPassphrase)
          .Wait());
  EXPECT_TRUE(ContactInfoActiveChecker(
                  GetSyncService(0),
                  /*expect_active=*/EnabledForPassphraseUsersTestParam())
                  .Wait());
}

// Specialized fixture that enables AutofillAccountProfilesOnSignIn.
class SingleClientContactInfoTransportSyncTest
    : public SingleClientContactInfoSyncTest {
 public:
  SingleClientContactInfoTransportSyncTest() {
    transport_feature_.InitWithFeatures(
        /*enabled_features=*/{syncer::
                                  kSyncEnableContactInfoDataTypeInTransportMode,
                              switches::kExplicitBrowserSigninUIOnDesktop},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList transport_feature_;
};

// When SyncEnableContactInfoDataTypeInTransportMode is enabled, the
// CONTACT_INFO type should run in transport mode and the availability of
// account profiles should depend on the signed-in state.
IN_PROC_BROWSER_TEST_F(SingleClientContactInfoTransportSyncTest,
                       TransportMode) {
  AutofillProfile profile = BuildTestAccountProfile();
  AddSpecificsToServer(AsContactInfoSpecifics(profile), GetFakeServer());
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  EXPECT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(),
                  UnorderedElementsAre(profile))
                  .Wait());
  // ChromeOS doesn't have the concept of sign-out.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(), IsEmpty())
                  .Wait());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientContactInfoTransportSyncTest,
                       DeleteAccountDataInErrorState) {
  // Add a profile to account storage.
  AutofillProfile profile = BuildTestAccountProfile();
  AddSpecificsToServer(AsContactInfoSpecifics(profile), GetFakeServer());
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  EXPECT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(),
                  UnorderedElementsAre(profile))
                  .Wait());

  // Trigger auth error, sync stops.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile(0));
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_TRUE(ContactInfoActiveChecker(GetSyncService(0),
                                       /*expect_active=*/false)
                  .Wait());

  // The data has been deleted.
  EXPECT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(), IsEmpty())
                  .Wait());

  // Fix the error, data is re-downloaded.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      GoogleServiceAuthError::AuthErrorNone());

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  EXPECT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(),
                  UnorderedElementsAre(profile))
                  .Wait());
}

// Account storage is not enabled when the user is in auth error.
IN_PROC_BROWSER_TEST_F(SingleClientContactInfoTransportSyncTest,
                       AuthErrorState) {
  // Setup transport mode.
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));

  // Trigger auth error, sync stops.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile(0));
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_TRUE(ContactInfoActiveChecker(GetSyncService(0),
                                       /*expect_active=*/false)
                  .Wait());

  EXPECT_FALSE(GetPersonalDataManager()
                   ->address_data_manager()
                   .IsEligibleForAddressAccountStorage());
  EXPECT_FALSE(GetPersonalDataManager()
                   ->address_data_manager()
                   .IsAutofillSyncToggleAvailable());

  // Fix the authentication error, sync is available again.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      GoogleServiceAuthError::AuthErrorNone());
  EXPECT_TRUE(ContactInfoActiveChecker(GetSyncService(0),
                                       /*expect_active=*/true)
                  .Wait());
  EXPECT_TRUE(GetPersonalDataManager()
                  ->address_data_manager()
                  .IsEligibleForAddressAccountStorage());
  EXPECT_TRUE(GetPersonalDataManager()
                  ->address_data_manager()
                  .IsAutofillSyncToggleAvailable());
}

// Regression test for https://crbug.com/340194452
IN_PROC_BROWSER_TEST_F(SingleClientContactInfoTransportSyncTest,
                       IsAutofillSyncToggleAvailable) {
  // Setup transport mode.
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  // The toggle is available.
  EXPECT_TRUE(GetPersonalDataManager()
                  ->address_data_manager()
                  .IsAutofillSyncToggleAvailable());

  // Turn account storage OFF.
  GetPersonalDataManager()
      ->address_data_manager()
      .SetAutofillSelectableTypeEnabled(
          /*enabled=*/false);
  EXPECT_TRUE(ContactInfoActiveChecker(GetSyncService(0),
                                       /*expect_active=*/false)
                  .Wait());

  // The toggle is still available.
  EXPECT_TRUE(GetPersonalDataManager()
                  ->address_data_manager()
                  .IsAutofillSyncToggleAvailable());

  // Turn on Sync.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount(signin::ConsentLevel::kSync));

  // The toggle is no longer available.
  EXPECT_FALSE(GetPersonalDataManager()
                   ->address_data_manager()
                   .IsAutofillSyncToggleAvailable());

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Sign out.
  GetClient(0)->SignOutPrimaryAccount();

  // The toggle is not available.
  EXPECT_FALSE(GetPersonalDataManager()
                   ->address_data_manager()
                   .IsAutofillSyncToggleAvailable());
#endif
}

IN_PROC_BROWSER_TEST_F(SingleClientContactInfoSyncTest,
                       PreservesUnsupportedFieldsDataOnCommits) {
  // Create an unsupported field with an unused tag.
  const std::string kUnsupportedField =
      CreateSerializedProtoField(/*field_number=*/999999, "unknown_field");

  autofill::AutofillProfile profile = BuildTestAccountProfile();
  profile.SetRawInfo(autofill::NAME_FULL, u"Full Name");
  profile.FinalizeAfterImport();

  sync_pb::EntitySpecifics entity_data;
  sync_pb::ContactInfoSpecifics* specifics = entity_data.mutable_contact_info();
  *specifics = autofill::ContactInfoSpecificsFromAutofillProfile(profile, {});
  *specifics->mutable_unknown_fields() = kUnsupportedField;
  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"",
          /*client_tag=*/
          profile.guid(), entity_data,
          /*creation_time=*/0,
          /*last_modified_time=*/0));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  ASSERT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(),
                  UnorderedElementsAre(profile))
                  .Wait());

  // Apply a change to the profile.
  profile.SetRawInfo(autofill::NAME_FULL, u"New Name");
  GetPersonalDataManager()->address_data_manager().UpdateProfile(profile);

  // Add a second profile to make sure that the server receives the update.
  autofill::AutofillProfile profile2 = BuildTestAccountProfile();
  GetPersonalDataManager()->address_data_manager().AddProfile(profile2);

  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::CONTACT_INFO, 2).Wait());
  // Verifies that `profile` has preserved unknown_fields.
  EXPECT_THAT(fake_server_->GetSyncEntitiesByDataType(syncer::CONTACT_INFO),
              UnorderedElementsAre(
                  HasContactInfoWithGuidAndUnknownFields(profile.guid(),
                                                         kUnsupportedField),
                  HasContactInfoWithGuidAndUnknownFields(profile2.guid(), "")));
}

// Overwrite the Sync test account with a non-gmail account. This treats it as a
// Dasher account.
// On Android, `switches::kSyncUserForTest` isn't supported, so it's currently
// not possible to simulate a non-gmail account.
class SingleClientContactInfoManagedAccountTest
    : public SingleClientContactInfoSyncTest {
 public:
  SingleClientContactInfoManagedAccountTest() {
    // This can't be done in `SetUpCommandLine()` because `SyncTest::SetUp()`
    // already consumes the parameter.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSyncUserForTest, "user@managed-domain.com");
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientContactInfoManagedAccountTest,
                       DisabledForManagedAccounts) {
  ASSERT_TRUE(SetupClients());
  // Sign in with a managed account.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount(signin::ConsentLevel::kSync));
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile(0));
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  signin::SimulateSuccessfulFetchOfAccountInfo(
      identity_manager, account.account_id, account.email, account.gaia,
      "managed-domain.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");
  ASSERT_TRUE(SetupSync());

  EXPECT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
}
#endif

}  // namespace
