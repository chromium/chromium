// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using autofill::AutofillTable;
using autofill::AutofillProfile;
using autofill::AutofillType;
using autofill::CreditCard;
using autofill::PersonalDataManager;
using autofill_helper::AddProfile;
using autofill_helper::CreateAutofillProfile;
using autofill_helper::CreateUniqueAutofillProfile;
using autofill_helper::GetAllAutoFillProfiles;
using autofill_helper::GetPersonalDataManager;
using autofill_helper::GetProfileCount;
using autofill_helper::ProfilesMatch;
using autofill_helper::PROFILE_FRASIER;
using autofill_helper::PROFILE_HOMER;
using autofill_helper::PROFILE_MARION;
using autofill_helper::PROFILE_NULL;
using autofill_helper::RemoveProfile;
using autofill_helper::SetCreditCards;
using autofill_helper::UpdateProfile;

// Class that enables or disables USS based on test parameter. Must be the first
// base class of the test fixture.
// TODO(jkrcal): When the new implementation fully launches, remove this class,
// convert all tests from *_P back to *_F and remove the instance at the end.
class UssSwitchToggler : public testing::WithParamInterface<bool> {
 public:
  UssSwitchToggler() {
    if (GetParam()) {
      override_features_.InitAndEnableFeature(
          switches::kSyncUSSAutofillProfile);
    } else {
      override_features_.InitAndDisableFeature(
          switches::kSyncUSSAutofillProfile);
    }
  }

 private:
  base::test::ScopedFeatureList override_features_;
};

class TwoClientAutofillProfileSyncTest : public UssSwitchToggler,
                                         public SyncTest {
 public:
  TwoClientAutofillProfileSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientAutofillProfileSyncTest() override {}

  bool TestUsesSelfNotifications() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientAutofillProfileSyncTest);
};

IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest,
                       PersonalDataManagerSanity) {
  ASSERT_TRUE(SetupSync());

  // Client0 adds a profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());

  // Client1 adds a profile.
  AddProfile(1, CreateAutofillProfile(PROFILE_MARION));
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(2U, GetAllAutoFillProfiles(0).size());

  // Client0 adds the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_MARION));
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(2U, GetAllAutoFillProfiles(0).size());

  // Client1 removes a profile.
  RemoveProfile(1, GetAllAutoFillProfiles(1)[0]->guid());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());

  // Client0 updates a profile.
  UpdateProfile(0,
                GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::NAME_FIRST),
                base::ASCIIToUTF16("Bart"));
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());

  // Client1 removes remaining profile.
  RemoveProfile(1, GetAllAutoFillProfiles(1)[0]->guid());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(0U, GetAllAutoFillProfiles(0).size());
}

IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest, AddDuplicateProfiles) {
  ASSERT_TRUE(SetupClients());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());
}

IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest,
                       SameProfileWithConflict) {
  ASSERT_TRUE(SetupClients());

  AutofillProfile profile0 = CreateAutofillProfile(PROFILE_HOMER);
  AutofillProfile profile1 = CreateAutofillProfile(PROFILE_HOMER);
  profile1.SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER,
                      base::ASCIIToUTF16("1234567890"));

  AddProfile(0, profile0);
  AddProfile(1, profile1);
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());
}

// Tests that a null profile does not get synced across clients.
IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest, AddEmptyProfile) {
  ASSERT_TRUE(SetupSync());

  AddProfile(0, CreateAutofillProfile(PROFILE_NULL));
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(0U, GetAllAutoFillProfiles(0).size());
}

// Tests that adding a profile on one client results in it being added on the
// other client when sync is running.
IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest, AddProfile) {
  ASSERT_TRUE(SetupSync());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));

  // Wait for the sync to happen.
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());

  // Make sure that both clients have one profile.
  EXPECT_EQ(1U, GetProfileCount(0));
  EXPECT_EQ(1U, GetProfileCount(1));
}

// Tests that adding a profile on one client results in it being added on the
// other client when sync gets started.
IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest,
                       AddProfile_BeforeSyncStart) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed";

  // Add the new autofill profile before starting sync.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(SetupSync());

  // Wait for the sync to happen.
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());

  // Make sure that both clients have one profile.
  EXPECT_EQ(1U, GetProfileCount(0));
  EXPECT_EQ(1U, GetProfileCount(1));
}

// Tests that adding the same profile on the two clients before sync is started
// results in each client only having one profile after sync is started
IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest,
                       ClientsAddSameProfile) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed";

  // Add the same profile in the two clients.
  AddProfile(0, CreateUniqueAutofillProfile());
  AddProfile(1, CreateUniqueAutofillProfile());

  // Make sure the GUIDs are different.
  ASSERT_NE(GetAllAutoFillProfiles(0)[0]->guid(),
            GetAllAutoFillProfiles(1)[0]->guid());

  // Wait for the sync to happen.
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());

  // Make sure that both clients have one profile.
  EXPECT_EQ(1U, GetProfileCount(0));
  EXPECT_EQ(1U, GetProfileCount(1));

  // Make sure that they have the same GUID.
  EXPECT_EQ(GetAllAutoFillProfiles(0)[0]->guid(),
            GetAllAutoFillProfiles(1)[0]->guid());
}

// Tests that adding multiple profiles to one client results in all of them
// being added to the other client.
IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest,
                       AddMultipleProfilesOnOneClient) {
  ASSERT_TRUE(SetupClients());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(0, CreateAutofillProfile(PROFILE_MARION));
  AddProfile(0, CreateAutofillProfile(PROFILE_FRASIER));
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(3U, GetAllAutoFillProfiles(0).size());
}

// Tests that adding multiple profiles to two client results both clients having
// all profiles.
IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest,
                       AddMultipleProfilesOnTwoClients) {
  ASSERT_TRUE(SetupClients());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(1, CreateAutofillProfile(PROFILE_MARION));
  AddProfile(1, CreateAutofillProfile(PROFILE_FRASIER));
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(3U, GetAllAutoFillProfiles(0).size());
}

// Tests that deleting a profile on one client results in it being deleted on
// the other client.
IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest, DeleteProfile) {
  ASSERT_TRUE(SetupSync());

  // Setup the test by making the 2 clients have the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());

  // Remove the profile from one client.
  RemoveProfile(1, GetAllAutoFillProfiles(1)[0]->guid());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());

  // Make sure both clients don't have any profiles.
  EXPECT_EQ(0U, GetAllAutoFillProfiles(0).size());
}

// Tests that modifying a profile while syncing results in the other client
// getting the updated profile.
IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest, UpdateFields) {
  ASSERT_TRUE(SetupSync());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());

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
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());
  EXPECT_EQ(new_name,
            base::UTF16ToUTF8(GetAllAutoFillProfiles(1)[0]->GetRawInfo(
                autofill::NAME_FIRST)));
  EXPECT_EQ(new_email,
            base::UTF16ToUTF8(GetAllAutoFillProfiles(1)[0]->GetRawInfo(
                autofill::EMAIL_ADDRESS)));
}

// Tests that modifying a profile at the same time one two clients while
// syncing results in the both client having the same profile (doesn't matter
// which one).
IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest,
                       UpdateConflictingFields) {
  ASSERT_TRUE(SetupSync());

  // Make the two clients have the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());

  // Update the same field differently on the two clients at the same time.
  UpdateProfile(0,
                GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::NAME_FIRST),
                base::ASCIIToUTF16("Lisa"));
  UpdateProfile(1, GetAllAutoFillProfiles(1)[0]->guid(),
                AutofillType(autofill::NAME_FIRST), base::ASCIIToUTF16("Bart"));

  // Don't care which write wins the conflict, only that the two clients agree.
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());
}

// Tests that modifying a profile at the same time one two clients while
// syncing results in the both client having the same profile (doesn't matter
// which one).
IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest,
                       UpdateConflictingFieldsDuringInitialMerge) {
  ASSERT_TRUE(SetupClients());

  // Make the two clients have the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(1, CreateAutofillProfile(PROFILE_HOMER));

  // Update the same field differently on the two clients at the same time.
  UpdateProfile(0, GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::NAME_FIRST), base::ASCIIToUTF16("Lisa"));
  UpdateProfile(1, GetAllAutoFillProfiles(1)[0]->guid(),
                AutofillType(autofill::NAME_FIRST), base::ASCIIToUTF16("Bart"));

  // Start sync.
  ASSERT_TRUE(SetupSync());

  // Don't care which write wins the conflict, only that the two clients agree.
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());
}

// Tests that modifying a profile at the same time on two clients while
// syncing results in both client having the same profile (doesn't matter which
// one).
IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest, DeleteAndUpdate) {
  ASSERT_TRUE(SetupSync());

  // Make the two clients have the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(AutofillProfileChecker(0, 1).Wait());
  ASSERT_EQ(1U, GetAllAutoFillProfiles(0).size());

  RemoveProfile(0, GetAllAutoFillProfiles(0)[0]->guid());
  UpdateProfile(1, GetAllAutoFillProfiles(1)[0]->guid(),
                AutofillType(autofill::NAME_FIRST), base::ASCIIToUTF16("Bart"));

  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  // The exact result is non-deterministic without a strong consistency model
  // server-side, but both clients should converge (either update or delete).
  EXPECT_EQ(GetAllAutoFillProfiles(0).size(), GetAllAutoFillProfiles(1).size());
}

// Tests that modifying a profile at the same time on two clients while
// syncing results in a conflict where the update wins. This only works with
// a server that supports a strong consistency model and is hence capable of
// detecting conflicts server-side.
IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest,
                       DeleteAndUpdateWithStrongConsistency) {
  if (GetParam() == false) {
    // TODO(crbug.com/890746): There seems to be a bug in directory code that
    // resolves conflicts in a way that local deletion wins over a remote
    // update, which makes this test non-deterministic, because the logic is
    // asymmetric (so the outcome depends on which client commits first).
    // For now, we "disable" the test.
    return;
  }

  ASSERT_TRUE(SetupSync());
  GetFakeServer()->EnableStrongConsistencyWithConflictDetectionModel();

  // Make the two clients have the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(AutofillProfileChecker(0, 1).Wait());
  ASSERT_EQ(1U, GetAllAutoFillProfiles(0).size());

  RemoveProfile(0, GetAllAutoFillProfiles(0)[0]->guid());
  UpdateProfile(1, GetAllAutoFillProfiles(1)[0]->guid(),
                AutofillType(autofill::NAME_FIRST), base::ASCIIToUTF16("Bart"));

  // One of the two clients (the second one committing) will be requested by the
  // server to resolve the conflict and recommit. The conflict resolution should
  // be undeletion wins, which can mean local wins or remote wins, depending on
  // which client is involved.
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(1).size());
}

IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest, MaxLength) {
  ASSERT_TRUE(SetupSync());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(AutofillProfileChecker(0, 1).Wait());
  ASSERT_EQ(1U, GetAllAutoFillProfiles(0).size());

  base::string16 max_length_string(AutofillTable::kMaxDataLength, '.');
  UpdateProfile(0,
                GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::NAME_FULL),
                max_length_string);
  UpdateProfile(0,
                GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::EMAIL_ADDRESS),
                max_length_string);
  UpdateProfile(0,
                GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::ADDRESS_HOME_LINE1),
                max_length_string);

  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest, ExceedsMaxLength) {
  ASSERT_TRUE(SetupSync());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(AutofillProfileChecker(0, 1).Wait());
  ASSERT_EQ(1U, GetAllAutoFillProfiles(0).size());

  base::string16 exceeds_max_length_string(
      AutofillTable::kMaxDataLength + 1, '.');
  UpdateProfile(0,
                GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::NAME_FIRST),
                exceeds_max_length_string);
  UpdateProfile(0,
                GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::NAME_LAST),
                exceeds_max_length_string);
  UpdateProfile(0,
                GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::EMAIL_ADDRESS),
                exceeds_max_length_string);
  UpdateProfile(0,
                GetAllAutoFillProfiles(0)[0]->guid(),
                AutofillType(autofill::ADDRESS_HOME_LINE1),
                exceeds_max_length_string);

  ASSERT_TRUE(AwaitQuiescence());
  EXPECT_FALSE(ProfilesMatch(0, 1));
}

// Test credit cards don't sync.
IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest, NoCreditCardSync) {
  ASSERT_TRUE(SetupSync());

  CreditCard card;
  card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                  base::ASCIIToUTF16("6011111111111117"));
  std::vector<CreditCard> credit_cards{card};
  SetCreditCards(0, &credit_cards);

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));

  // Because the credit card was created before the profile, if we wait for the
  // profile to sync between both clients, it should give the credit card enough
  // time to sync. We cannot directly wait/block for the credit card to sync
  // because we're expecting it to not sync.
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());

  PersonalDataManager* pdm = GetPersonalDataManager(1);
  EXPECT_EQ(0U, pdm->GetCreditCards().size());
}

IN_PROC_BROWSER_TEST_P(TwoClientAutofillProfileSyncTest,
                       E2E_ONLY(TwoClientsAddAutofillProfiles)) {
  ASSERT_TRUE(SetupSync());

  // All profiles should sync same autofill profiles.
  ASSERT_TRUE(AutofillProfileChecker(0, 1).Wait())
      << "Initial autofill profiles did not match for all profiles.";

  // For clean profiles, the autofill profiles count should be zero. We are not
  // enforcing this, we only check that the final count is equal to initial
  // count plus new autofill profiles count.
  size_t init_autofill_profiles_count = GetProfileCount(0);

  // Add a new autofill profile to the first client.
  AddProfile(0, CreateUniqueAutofillProfile());

  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());

  // Check that the total number of autofill profiles is as expected
  for (int i = 0; i < num_clients(); ++i) {
    EXPECT_EQ(GetProfileCount(i), init_autofill_profiles_count + 1U)
        << "Total autofill profile count is wrong.";
  }
}

// Only parametrize the tests above that test autofill_profile, the tests below
// address autocomplete and thus do not need parametrizing.
INSTANTIATE_TEST_CASE_P(USS,
                        TwoClientAutofillProfileSyncTest,
                        ::testing::Values(false, true));

}  // namespace
