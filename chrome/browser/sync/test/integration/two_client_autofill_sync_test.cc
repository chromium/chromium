// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using autofill::AutofillProfile;
using autofill::AutofillTable;
using autofill::AutofillType;
using autofill::CreditCard;
using autofill::PersonalDataManager;
using autofill_helper::AddProfile;
using autofill_helper::CreateAutofillProfile;
using autofill_helper::CreateUniqueAutofillProfile;
using autofill_helper::GetAllAutoFillProfiles;
using autofill_helper::GetPersonalDataManager;
using autofill_helper::GetProfileCount;
using autofill_helper::PROFILE_FRASIER;
using autofill_helper::PROFILE_HOMER;
using autofill_helper::PROFILE_MARION;
using autofill_helper::PROFILE_NULL;
using autofill_helper::ProfilesMatch;
using autofill_helper::RemoveProfile;
using autofill_helper::SetCreditCards;
using autofill_helper::UpdateProfile;

class TwoClientAutofillProfileSyncTest : public SyncTest {
 public:
  TwoClientAutofillProfileSyncTest() : SyncTest(TWO_CLIENT) {}

  TwoClientAutofillProfileSyncTest(const TwoClientAutofillProfileSyncTest&) =
      delete;
  TwoClientAutofillProfileSyncTest& operator=(
      const TwoClientAutofillProfileSyncTest&) = delete;

  ~TwoClientAutofillProfileSyncTest() override = default;
};

#if BUILDFLAG(IS_MAC)
#define MAYBE_PersonalDataManagerSanity DISABLED_PersonalDataManagerSanity
#else
#define MAYBE_PersonalDataManagerSanity PersonalDataManagerSanity
#endif
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       MAYBE_PersonalDataManagerSanity) {
  ASSERT_TRUE(SetupSync());

  base::HistogramTester histograms;

  // Client0 adds a profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  // Client1 adds a profile.
  AddProfile(1, CreateAutofillProfile(PROFILE_MARION));
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/2U).Wait());

  // Client0 adds the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_MARION));
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/2U).Wait());

  // Client1 removes a profile.
  RemoveProfile(1, GetAllAutoFillProfiles(1)[0]->guid());
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  // Client0 updates a profile.
  UpdateProfile(0, GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::NAME_FIRST), u"Bart");
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  // Client1 removes remaining profile.
  RemoveProfile(1, GetAllAutoFillProfiles(1)[0]->guid());
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/0U).Wait());

  // Client1 adds a final new profile.
  AddProfile(1, CreateAutofillProfile(PROFILE_FRASIER));
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  // Each of the clients deletes one profile.
  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               syncer::ModelTypeEntityChange::kLocalDeletion,
                               2);
}

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       SyncHistogramsInitialSync) {
  ASSERT_TRUE(SetupClients());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_EQ(1U, GetAllAutoFillProfiles(0).size());

  AddProfile(1, CreateAutofillProfile(PROFILE_MARION));
  ASSERT_EQ(1U, GetAllAutoFillProfiles(1).size());

  base::HistogramTester histograms;

  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/2U).Wait());

  // The order of events is roughly: First client (whichever that happens to be)
  // connects to the server, commits its entity, and gets no initial updates
  // because nothing was on the server yet. Then the second client connects,
  // commits its entity, and receives the first client's entity as an initial
  // update. Then the first client receives the second's entity as a non-initial
  // update. And finally, at some point, each client receives its own entity
  // back as a non-initial update, for a total of 1 initial and 3 non-initial
  // updates.
  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               syncer::ModelTypeEntityChange::kLocalCreation,
                               2);
  histograms.ExpectBucketCount(
      "Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
      syncer::ModelTypeEntityChange::kRemoteInitialUpdate, 1);
  histograms.ExpectBucketCount(
      "Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
      syncer::ModelTypeEntityChange::kRemoteNonInitialUpdate, 3);
  histograms.ExpectTotalCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                              6);
}

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, AddDuplicateProfiles) {
  ASSERT_TRUE(SetupClients());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());
}

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       SameProfileWithConflict) {
  ASSERT_TRUE(SetupClients());

  AutofillProfile profile0 = CreateAutofillProfile(PROFILE_HOMER);
  AutofillProfile profile1 = CreateAutofillProfile(PROFILE_HOMER);
  profile1.SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER, u"1234567890");

  AddProfile(0, profile0);
  AddProfile(1, profile1);
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       AddDuplicateProfiles_OneIsVerified) {
  ASSERT_TRUE(SetupClients());

  // Create two identical profiles where one of them is verified, additionally.
  AutofillProfile profile0 = autofill::test::GetFullProfile();
  AutofillProfile profile1 =
      autofill::test::GetVerifiedProfile();  // I.e. Full + Verified.
  std::string verified_origin = profile1.origin();

  AddProfile(0, profile0);
  AddProfile(1, profile1);
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  EXPECT_EQ(verified_origin, GetAllAutoFillProfiles(0)[0]->origin());
  EXPECT_EQ(verified_origin, GetAllAutoFillProfiles(1)[0]->origin());
}

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       AddDuplicateProfiles_OneAtStart_OtherComesLater) {
  ASSERT_TRUE(SetupClients());

  AutofillProfile profile0 = autofill::test::GetFullProfile();
  AutofillProfile profile1 =
      autofill::test::GetVerifiedProfile();  // I.e. Full + Verified.
  std::string verified_origin = profile1.origin();

  AddProfile(0, profile0);
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  // Add the same (but verified) profile on the other client, afterwards.
  AddProfile(1, profile1);
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  // The latter addition is caught in deduplication logic in PDM to sync. As a
  // result, both clients end up with the non-verified profile.
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());

  EXPECT_NE(verified_origin, GetAllAutoFillProfiles(0)[0]->origin());
  EXPECT_NE(verified_origin, GetAllAutoFillProfiles(1)[0]->origin());
}

// Tests that a null profile does not get synced across clients.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, AddEmptyProfile) {
  ASSERT_TRUE(SetupSync());

  AddProfile(0, CreateAutofillProfile(PROFILE_NULL));
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/0U).Wait());
}

// Tests that adding a profile on one client results in it being added on the
// other client when sync is running.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, AddProfile) {
  ASSERT_TRUE(SetupSync());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));

  // Wait for the sync to happen and make sure both clients have one profile.
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());
}

// Tests that adding a profile on one client results in it being added on the
// other client when sync gets started.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       AddProfile_BeforeSyncStart) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed";

  // Add the new autofill profile before starting sync.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(SetupSync());

  // Wait for the sync to happen and make sure both clients have one profile.
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());
}

// Tests that adding the same profile on the two clients before sync is started
// results in each client only having one profile after sync is started
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       ClientsAddSameProfile) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed";

  // Add the same profile in the two clients.
  AddProfile(0, CreateUniqueAutofillProfile());
  AddProfile(1, CreateUniqueAutofillProfile());

  // Make sure the GUIDs are different.
  ASSERT_NE(GetAllAutoFillProfiles(0)[0]->guid(),
            GetAllAutoFillProfiles(1)[0]->guid());

  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  // Make sure that they have the same GUID.
  EXPECT_EQ(GetAllAutoFillProfiles(0)[0]->guid(),
            GetAllAutoFillProfiles(1)[0]->guid());
}

// Tests that adding multiple profiles to one client results in all of them
// being added to the other client.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       AddMultipleProfilesOnOneClient) {
  ASSERT_TRUE(SetupClients());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(0, CreateAutofillProfile(PROFILE_MARION));
  AddProfile(0, CreateAutofillProfile(PROFILE_FRASIER));
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/3U).Wait());
}

// Tests that adding multiple profiles to two client results both clients having
// all profiles.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       AddMultipleProfilesOnTwoClients) {
  ASSERT_TRUE(SetupClients());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(1, CreateAutofillProfile(PROFILE_MARION));
  AddProfile(1, CreateAutofillProfile(PROFILE_FRASIER));
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/3U).Wait());
}

// Tests that deleting a profile on one client results in it being deleted on
// the other client.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, DeleteProfile) {
  ASSERT_TRUE(SetupSync());

  // Setup the test by making the 2 clients have the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  // Remove the profile from one client.
  RemoveProfile(1, GetAllAutoFillProfiles(1)[0]->guid());
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/0U).Wait());
}

// Tests that modifying a profile while syncing results in the other client
// getting the updated profile.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, UpdateFields) {
  ASSERT_TRUE(SetupSync());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  // Modify the profile on one client.
  std::string new_name = "Lisa";
  std::string new_email = "grrrl@TV.com";
  UpdateProfile(0, GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::NAME_FIRST),
                base::ASCIIToUTF16(new_name));
  UpdateProfile(0, GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::EMAIL_ADDRESS),
                base::ASCIIToUTF16(new_email));

  // Make sure the change is propagated to the other client.
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());
  EXPECT_EQ(new_name,
            base::UTF16ToUTF8(GetAllAutoFillProfiles(1)[0]->GetRawInfo(
                autofill::NAME_FIRST)));
  EXPECT_EQ(new_email,
            base::UTF16ToUTF8(GetAllAutoFillProfiles(1)[0]->GetRawInfo(
                autofill::EMAIL_ADDRESS)));
}

// Tests that modifying the verification status of a token in the profile will
// be propagated.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       UpdateVerificationStatus) {
  ASSERT_TRUE(SetupSync());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  AutofillProfile* profile = GetAllAutoFillProfiles(0)[0];
  ASSERT_TRUE(profile);
  UpdateProfile(
      0, profile->guid(), AutofillType(autofill::NAME_FIRST),
      profile->GetRawInfo(autofill::NAME_FIRST),
      autofill::structured_address::VerificationStatus::kUserVerified);

  // Make sure the change is propagated to the other client.
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());
  EXPECT_EQ(autofill::structured_address::VerificationStatus::kUserVerified,
            GetAllAutoFillProfiles(1)[0]->GetVerificationStatus(
                autofill::NAME_FIRST));
}

// Tests that modifying a profile at the same time one two clients while
// syncing results in the both client having the same profile (doesn't matter
// which one).
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       UpdateConflictingFields) {
  ASSERT_TRUE(SetupSync());

  // Make the two clients have the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  // Update the same field differently on the two clients at the same time.
  UpdateProfile(0, GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::NAME_FIRST), u"Lisa");
  UpdateProfile(1, GetAllAutoFillProfiles(1)[0]->guid(),
                AutofillType(autofill::NAME_FIRST), u"Bart");

  // Don't care which write wins the conflict, only that the two clients agree.
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());
}

// Tests that modifying a profile at the same time one two clients while
// syncing results in the both client having the same profile (doesn't matter
// which one).
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       UpdateConflictingFieldsDuringInitialMerge) {
  ASSERT_TRUE(SetupClients());

  // Make the two clients have the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(1, CreateAutofillProfile(PROFILE_HOMER));

  // Update the same field differently on the two clients at the same time.
  UpdateProfile(0, GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::NAME_FIRST), u"Lisa");
  UpdateProfile(1, GetAllAutoFillProfiles(1)[0]->guid(),
                AutofillType(autofill::NAME_FIRST), u"Bart");

  // Start sync.
  ASSERT_TRUE(SetupSync());

  // Don't care which write wins the conflict, only that the two clients agree.
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());
}

// Tests that modifying a profile at the same time on two clients while
// syncing results in both client having the same profile (doesn't matter which
// one).
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, DeleteAndUpdate) {
  ASSERT_TRUE(SetupSync());

  // Make the two clients have the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  RemoveProfile(0, GetAllAutoFillProfiles(0)[0]->guid());
  UpdateProfile(1, GetAllAutoFillProfiles(1)[0]->guid(),
                AutofillType(autofill::NAME_FIRST), u"Bart");

  EXPECT_TRUE(AutofillProfileChecker(0, 1, absl::nullopt).Wait());
  // The exact result is non-deterministic without a strong consistency model
  // server-side, but both clients should converge (either update or delete).
  EXPECT_EQ(GetAllAutoFillProfiles(0).size(), GetAllAutoFillProfiles(1).size());
}

// Tests that modifying a profile at the same time on two clients while
// syncing results in a conflict where the update wins. This only works with
// a server that supports a strong consistency model and is hence capable of
// detecting conflicts server-side.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       DeleteAndUpdateWithStrongConsistency) {
  ASSERT_TRUE(SetupSync());
  GetFakeServer()->EnableStrongConsistencyWithConflictDetectionModel();

  // Make the two clients have the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  RemoveProfile(0, GetAllAutoFillProfiles(0)[0]->guid());
  UpdateProfile(1, GetAllAutoFillProfiles(1)[0]->guid(),
                AutofillType(autofill::NAME_FIRST), u"Bart");

  // One of the two clients (the second one committing) will be requested by the
  // server to resolve the conflict and recommit. The conflict resolution should
  // be undeletion wins, which can mean local wins or remote wins, depending on
  // which client is involved.
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, MaxLength) {
  ASSERT_TRUE(SetupSync());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  std::u16string max_length_string(AutofillTable::kMaxDataLength, '.');
  UpdateProfile(0, GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::NAME_FULL), max_length_string);
  UpdateProfile(0, GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::EMAIL_ADDRESS), max_length_string);
  UpdateProfile(0, GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::ADDRESS_HOME_LINE1), max_length_string);

  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, ExceedsMaxLength) {
  ASSERT_TRUE(SetupSync());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  std::u16string exceeds_max_length_string(AutofillTable::kMaxDataLength + 1,
                                           '.');
  UpdateProfile(0, GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::NAME_FIRST), exceeds_max_length_string);
  UpdateProfile(0, GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::NAME_LAST), exceeds_max_length_string);
  UpdateProfile(0, GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::EMAIL_ADDRESS),
                exceeds_max_length_string);
  UpdateProfile(0, GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::ADDRESS_HOME_LINE1),
                exceeds_max_length_string);

  ASSERT_TRUE(AwaitQuiescence());
  EXPECT_FALSE(ProfilesMatch(0, 1));
}

// Test credit cards don't sync.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, NoCreditCardSync) {
  ASSERT_TRUE(SetupSync());

  CreditCard card;
  card.SetRawInfo(autofill::CREDIT_CARD_NUMBER, u"6011111111111117");
  std::vector<CreditCard> credit_cards{card};
  SetCreditCards(0, &credit_cards);

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));

  // Because the credit card was created before the profile, if we wait for the
  // profile to sync between both clients, it should give the credit card enough
  // time to sync. We cannot directly wait/block for the credit card to sync
  // because we're expecting it to not sync.
  EXPECT_TRUE(AutofillProfileChecker(0, 1, /*expected_count=*/1U).Wait());

  PersonalDataManager* pdm = GetPersonalDataManager(1);
  EXPECT_EQ(0U, pdm->GetCreditCards().size());
}

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       E2E_ONLY(TwoClientsAddAutofillProfiles)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());

  // All profiles should sync same autofill profiles.
  ASSERT_TRUE(
      AutofillProfileChecker(0, 1, /*expected_count=*/absl::nullopt).Wait())
      << "Initial autofill profiles did not match for all profiles.";

  // For clean profiles, the autofill profiles count should be zero. We are not
  // enforcing this, we only check that the final count is equal to initial
  // count plus new autofill profiles count.
  size_t init_autofill_profiles_count = GetProfileCount(0);

  // Add a new autofill profile to the first client.
  AddProfile(0, CreateUniqueAutofillProfile());

  EXPECT_TRUE(AutofillProfileChecker(
                  0, 1, /*expected_count=*/init_autofill_profiles_count + 1)
                  .Wait());
}

}  // namespace
