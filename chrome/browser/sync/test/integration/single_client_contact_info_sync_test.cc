// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/contact_info_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/addresses/contact_info_sync_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/contact_info_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/fake_server.h"
#include "components/sync/test/test_matchers.h"
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
using syncer::MatchesLocalDataDescription;
using syncer::MatchesLocalDataItemModel;
using testing::_;
using testing::ElementsAre;
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

#if !BUILDFLAG(IS_CHROMEOS)
// Matches a sync::entity_data has a contact info field with `address`.
MATCHER_P(HasContactInfoWithAddress, address, "") {
  return base::UTF8ToUTF16(
             arg.specifics().contact_info().address_street_address().value()) ==
         address;
}

// Matches a AutofillProfile has a `autofill::FieldType::NAME_FIRST` with
// `first_name`.
MATCHER_P(HasContactInfoWithFirstName, first_name, "") {
  return arg->GetRawInfo(autofill::FieldType::NAME_FIRST) == first_name;
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

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

class SingleClientContactInfoSyncTestBase : public SyncTest {
 public:
  explicit SingleClientContactInfoSyncTestBase(
      SyncTest::SetupSyncMode setup_sync_mode)
      : SyncTest(SINGLE_CLIENT) {
    if (setup_sync_mode == SetupSyncMode::kSyncTransportOnly) {
      feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }

  // In SINGLE_CLIENT tests, there's only a single PersonalDataManager.
  autofill::PersonalDataManager* GetPersonalDataManager() const {
    return contact_info_helper::GetPersonalDataManager(GetProfile(0));
  }

  bool SetupSyncAndHideAccountNameEmailProfile() {
    if (!SetupSync()) {
      return false;
    }
    HideAccountNameEmailProfile();
    return true;
  }

  void HideAccountNameEmailProfile() {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(GetProfile(0));
    autofill::test::HideAccountNameEmailProfile(
        GetProfile(0)->GetPrefs(), identity_manager->FindExtendedAccountInfo(
                                       identity_manager->GetPrimaryAccountInfo(
                                           signin::ConsentLevel::kSignin)));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SingleClientContactInfoSyncTest
    : public SingleClientContactInfoSyncTestBase,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientContactInfoSyncTest()
      : SingleClientContactInfoSyncTestBase(GetSetupSyncMode()) {}

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientContactInfoSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SingleClientContactInfoSyncTest, DownloadInitialData) {
  const AutofillProfile kProfile = BuildTestAccountProfile();
  AddSpecificsToServer(AsContactInfoSpecifics(kProfile), GetFakeServer());
  ASSERT_TRUE(SetupSyncAndHideAccountNameEmailProfile());
  EXPECT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(),
                  UnorderedElementsAre(kProfile))
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientContactInfoSyncTest, UploadProfile) {
  const AutofillProfile kProfile = BuildTestAccountProfile();
  ASSERT_TRUE(SetupSyncAndHideAccountNameEmailProfile());
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
IN_PROC_BROWSER_TEST_P(SingleClientContactInfoSyncTest, FinalizeAfterImport) {
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
  ASSERT_TRUE(SetupSyncAndHideAccountNameEmailProfile());
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
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(SingleClientContactInfoSyncTest, ClearOnSignout) {
  const AutofillProfile kProfile = BuildTestAccountProfile();
  AddSpecificsToServer(AsContactInfoSpecifics(kProfile), GetFakeServer());
  ASSERT_TRUE(SetupSyncAndHideAccountNameEmailProfile());
  ASSERT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(),
                  UnorderedElementsAre(kProfile))
                  .Wait());
  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(), IsEmpty())
                  .Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Specialized fixture to test the behavior for custom passphrase users with and
// without kSyncEnableContactInfoDataTypeForCustomPassphraseUsers enabled.
class SingleClientContactInfoPassphraseSyncTest
    : public SingleClientContactInfoSyncTestBase,
      public testing::WithParamInterface<
          std::tuple<SyncTest::SetupSyncMode, bool>> {
 public:
  SingleClientContactInfoPassphraseSyncTest()
      : SingleClientContactInfoSyncTestBase(GetSetupSyncMode()) {
    passphrase_feature_.InitWithFeatureState(
        syncer::kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
        EnabledForPassphraseUsersTestParam());
  }

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return std::get<0>(GetParam());
  }

  bool EnabledForPassphraseUsersTestParam() const {
    return std::get<1>(GetParam());
  }

 private:
  base::test::ScopedFeatureList passphrase_feature_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SingleClientContactInfoPassphraseSyncTest,
    testing::Combine(GetSyncTestModes(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<SyncTest::SetupSyncMode, bool>>&
           info) {
      return testing::PrintToString(std::get<0>(info.param)) +
             (std::get<1>(info.param) ? "_EnabledForCustomPassphrase"
                                      : "_DisabledForCustomPassphrase");
    });

// TODO(crbug.com/336993637): Flaky on Android, Mac ASan.
#if BUILDFLAG(IS_ANDROID) || (BUILDFLAG(IS_MAC) && defined(ADDRESS_SANITIZER))
#define MAYBE_Passphrase DISABLED_Passphrase
#else
#define MAYBE_Passphrase Passphrase
#endif
IN_PROC_BROWSER_TEST_P(SingleClientContactInfoPassphraseSyncTest,
                       MAYBE_Passphrase) {
  ASSERT_TRUE(SetupSyncAndHideAccountNameEmailProfile());
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

// Transport Mode is only supported on these platforms.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// CONTACT_INFO should be able to run in transport mode and the availability of
// account profiles should depend on the signed-in state.
IN_PROC_BROWSER_TEST_P(SingleClientContactInfoSyncTest, TransportMode) {
  AutofillProfile profile = BuildTestAccountProfile();
  AddSpecificsToServer(AsContactInfoSpecifics(profile), GetFakeServer());
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  HideAccountNameEmailProfile();
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  EXPECT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(),
                  UnorderedElementsAre(profile))
                  .Wait());
  // ChromeOS doesn't have the concept of sign-out.
  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(), IsEmpty())
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientContactInfoSyncTest,
                       DeleteAccountDataInErrorState) {
  // Add a profile to account storage.
  AutofillProfile profile = BuildTestAccountProfile();
  AddSpecificsToServer(AsContactInfoSpecifics(profile), GetFakeServer());
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  HideAccountNameEmailProfile();
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
IN_PROC_BROWSER_TEST_P(SingleClientContactInfoSyncTest, AuthErrorState) {
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

  // The toggle is not available when kReplaceSyncPromosWithSignInPromos is
  // enabled, and is instead available in the account settings page.
  const bool is_autofill_sync_toggle_available =
      !base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos);
  EXPECT_EQ(GetPersonalDataManager()
                ->address_data_manager()
                .IsAutofillSyncToggleAvailable(),
            is_autofill_sync_toggle_available);
}

// Regression test for https://crbug.com/340194452.
// TODO(crbug.com/40943238): Remove when `kReplaceSyncPromosWithSignInPromos` is
// enabled.
IN_PROC_BROWSER_TEST_P(SingleClientContactInfoSyncTest,
                       IsAutofillSyncToggleAvailable) {
  // Setup transport mode.
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));

  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    // The toggle is not available when
    // kReplaceSyncPromosWithSignInPromos is enabled, and is instead
    // available in the account settings page.
    EXPECT_FALSE(GetPersonalDataManager()
                     ->address_data_manager()
                     .IsAutofillSyncToggleAvailable());
    return;
  }

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
  ASSERT_TRUE(GetClient(0)->SetupSync());
  HideAccountNameEmailProfile();
  // The toggle is no longer available.
  EXPECT_FALSE(GetPersonalDataManager()
                   ->address_data_manager()
                   .IsAutofillSyncToggleAvailable());

  // Sign out.
  GetClient(0)->SignOutPrimaryAccount();

  // The toggle is not available.
  EXPECT_FALSE(GetPersonalDataManager()
                   ->address_data_manager()
                   .IsAutofillSyncToggleAvailable());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(SingleClientContactInfoSyncTest,
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

  ASSERT_TRUE(SetupSyncAndHideAccountNameEmailProfile());
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

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(SingleClientContactInfoSyncTest,
                       ShouldReturnLocalDataDescriptions) {
  ASSERT_TRUE(SetupClients());

  autofill::AutofillProfile profile1 = autofill::test::GetFullProfile();
  GetPersonalDataManager()->address_data_manager().AddProfile(profile1);

  autofill::AutofillProfile profile2 = autofill::test::GetFullProfile2();
  GetPersonalDataManager()->address_data_manager().AddProfile(profile2);

  ASSERT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(),
                  UnorderedElementsAre(profile1, profile2))
                  .Wait());
  ASSERT_THAT(
      GetPersonalDataManager()->address_data_manager().GetProfilesByRecordType(
          autofill::AutofillProfile::RecordType::kLocalOrSyncable),
      UnorderedElementsAre(HasContactInfoWithFirstName(profile1.GetRawInfo(
                               autofill::FieldType::NAME_FIRST)),
                           HasContactInfoWithFirstName(profile2.GetRawInfo(
                               autofill::FieldType::NAME_FIRST))));

  // Setup transport mode.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  HideAccountNameEmailProfile();

  EXPECT_THAT(
      GetClient(0)->GetLocalDataDescriptionAndWait(syncer::CONTACT_INFO),
      MatchesLocalDataDescription(
          syncer::DataType::CONTACT_INFO,
          UnorderedElementsAre(
              MatchesLocalDataItemModel(profile1.guid(),
                                        syncer::LocalDataItemModel::NoIcon(),
                                        /*title=*/_, /*subtitle=*/_),
              MatchesLocalDataItemModel(profile2.guid(),
                                        syncer::LocalDataItemModel::NoIcon(),
                                        /*title=*/_, /*subtitle=*/_)),
          // TODO(crbug.com/373568992): Merge Desktop and Mobile data
          // under common struct.
          /*item_count=*/0u, /*domains=*/IsEmpty(),
          /*domain_count=*/0u));
}

IN_PROC_BROWSER_TEST_P(SingleClientContactInfoSyncTest,
                       ShouldBatchUploadAllEntries) {
  ASSERT_TRUE(SetupClients());

  autofill::AutofillProfile profile1 = autofill::test::GetFullProfile();
  GetPersonalDataManager()->address_data_manager().AddProfile(profile1);

  autofill::AutofillProfile profile2 = autofill::test::GetFullProfile2();
  GetPersonalDataManager()->address_data_manager().AddProfile(profile2);

  ASSERT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(),
                  UnorderedElementsAre(profile1, profile2))
                  .Wait());
  ASSERT_THAT(
      GetPersonalDataManager()->address_data_manager().GetProfilesByRecordType(
          autofill::AutofillProfile::RecordType::kLocalOrSyncable),
      UnorderedElementsAre(HasContactInfoWithFirstName(profile1.GetRawInfo(
                               autofill::FieldType::NAME_FIRST)),
                           HasContactInfoWithFirstName(profile2.GetRawInfo(
                               autofill::FieldType::NAME_FIRST))));

  // Setup transport mode.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  HideAccountNameEmailProfile();

  GetSyncService(0)->TriggerLocalDataMigration({syncer::CONTACT_INFO});

  EXPECT_TRUE(ServerCountMatchStatusChecker(syncer::CONTACT_INFO, 2).Wait());
  EXPECT_THAT(fake_server_->GetSyncEntitiesByDataType(syncer::CONTACT_INFO),
              UnorderedElementsAre(
                  HasContactInfoWithAddress(profile1.GetRawInfo(
                      autofill::FieldType::ADDRESS_HOME_STREET_ADDRESS)),
                  HasContactInfoWithAddress(profile2.GetRawInfo(
                      autofill::FieldType::ADDRESS_HOME_STREET_ADDRESS))));

  EXPECT_THAT(
      GetPersonalDataManager()->address_data_manager().GetProfilesByRecordType(
          autofill::AutofillProfile::RecordType::kLocalOrSyncable),
      IsEmpty());

  EXPECT_THAT(
      GetPersonalDataManager()->address_data_manager().GetProfilesByRecordType(
          autofill::AutofillProfile::RecordType::kAccount),
      UnorderedElementsAre(HasContactInfoWithFirstName(profile1.GetRawInfo(
                               autofill::FieldType::NAME_FIRST)),
                           HasContactInfoWithFirstName(profile2.GetRawInfo(
                               autofill::FieldType::NAME_FIRST))));
}

IN_PROC_BROWSER_TEST_P(SingleClientContactInfoSyncTest,
                       ShouldBatchUploadSomeEntries) {
  ASSERT_TRUE(SetupClients());

  autofill::AutofillProfile profile1 = autofill::test::GetFullProfile();
  GetPersonalDataManager()->address_data_manager().AddProfile(profile1);

  autofill::AutofillProfile profile2 = autofill::test::GetFullProfile2();
  GetPersonalDataManager()->address_data_manager().AddProfile(profile2);

  ASSERT_TRUE(AddressDataManagerProfileChecker(
                  &GetPersonalDataManager()->address_data_manager(),
                  UnorderedElementsAre(profile1, profile2))
                  .Wait());
  ASSERT_THAT(
      GetPersonalDataManager()->address_data_manager().GetProfilesByRecordType(
          autofill::AutofillProfile::RecordType::kLocalOrSyncable),
      UnorderedElementsAre(HasContactInfoWithFirstName(profile1.GetRawInfo(
                               autofill::FieldType::NAME_FIRST)),
                           HasContactInfoWithFirstName(profile2.GetRawInfo(
                               autofill::FieldType::NAME_FIRST))));

  // Setup transport mode.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  HideAccountNameEmailProfile();

  GetSyncService(0)->TriggerLocalDataMigrationForItems(
      {{syncer::CONTACT_INFO, {profile1.guid()}}});

  EXPECT_TRUE(ServerCountMatchStatusChecker(syncer::CONTACT_INFO, 1).Wait());
  EXPECT_THAT(fake_server_->GetSyncEntitiesByDataType(syncer::CONTACT_INFO),
              ElementsAre(HasContactInfoWithAddress(profile1.GetRawInfo(
                  autofill::FieldType::ADDRESS_HOME_STREET_ADDRESS))));

  EXPECT_THAT(
      GetPersonalDataManager()->address_data_manager().GetProfilesByRecordType(
          autofill::AutofillProfile::RecordType::kLocalOrSyncable),
      ElementsAre(HasContactInfoWithFirstName(
          profile2.GetRawInfo(autofill::FieldType::NAME_FIRST))));

  EXPECT_THAT(
      GetPersonalDataManager()->address_data_manager().GetProfilesByRecordType(
          autofill::AutofillProfile::RecordType::kAccount),
      ElementsAre(HasContactInfoWithFirstName(
          profile1.GetRawInfo(autofill::FieldType::NAME_FIRST))));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(SingleClientContactInfoSyncTest,
                       DisabledForManagedAccounts) {
  // Sign in with a managed account.
  ASSERT_TRUE(SetupSync(SyncTestAccount::kEnterpriseAccount1));
  HideAccountNameEmailProfile();
  EXPECT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
}
#endif

}  // namespace
