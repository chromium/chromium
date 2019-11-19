// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
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

// Copied from data_type_debug_info_emitter.cc.
enum ModelTypeEntityChange {
  LOCAL_DELETION = 0,
  LOCAL_CREATION = 1,
  LOCAL_UPDATE = 2,
  REMOTE_DELETION = 3,
  REMOTE_NON_INITIAL_UPDATE = 4,
  REMOTE_INITIAL_UPDATE = 5,
  MODEL_TYPE_ENTITY_CHANGE_COUNT = 6
};

class TwoClientAutofillProfileSyncTest : public SyncTest {
 public:
  TwoClientAutofillProfileSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientAutofillProfileSyncTest() override {}

  // Tests that check Sync.ModelTypeEntityChange* histograms require
  // self-notifications. The reason is that every commit will eventually trigger
  // an incoming update on the same client, and without self-notifications we
  // have no good way to reliably trigger these updates.
  bool TestUsesSelfNotifications() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientAutofillProfileSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       PersonalDataManagerSanity) {
  ASSERT_TRUE(SetupSync());

  base::HistogramTester histograms;

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

  // Client1 adds a final new profile.
  AddProfile(1, CreateAutofillProfile(PROFILE_FRASIER));
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());

  // Each of the clients deletes one profile.
  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 2);
}

// Flaky on Linux/Win/ChromeOS only. http://crbug.com/997629
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_SyncHistogramsInitialSync DISABLED_SyncHistogramsInitialSync
#else
#define MAYBE_SyncHistogramsInitialSync SyncHistogramsInitialSync
#endif
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       MAYBE_SyncHistogramsInitialSync) {
  ASSERT_TRUE(SetupClients());

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_EQ(1U, GetAllAutoFillProfiles(0).size());

  AddProfile(1, CreateAutofillProfile(PROFILE_MARION));
  ASSERT_EQ(1U, GetAllAutoFillProfiles(1).size());

  base::HistogramTester histograms;

  // Commit sequentially to make sure there is no race condition.
  SetupSyncOneClientAfterAnother();
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());

  ASSERT_EQ(2U, GetAllAutoFillProfiles(0).size());

  // The order of events is roughly: First client (whichever that happens to be)
  // connects to the server, commits its entity, and gets no initial updates
  // because nothing was on the server yet. Then the second client connects,
  // commits its entity, and receives the first client's entity as an initial
  // update. Then the first client receives the second's entity as a non-initial
  // update. And finally, at some point, each client receives its own entity
  // back as a non-initial update, for a total of 1 initial and 3 non-initial
  // updates.
  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_CREATION, 2);
  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               REMOTE_INITIAL_UPDATE, 1);
  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               REMOTE_NON_INITIAL_UPDATE, 3);
  histograms.ExpectTotalCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                              6);
}

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, AddDuplicateProfiles) {
  ASSERT_TRUE(SetupClients());
  // TODO(crbug.com/904390): Once the investigation is over, remove the
  // histogram checks for zero LOCAL_DELETIONS here and in all further tests.
  base::HistogramTester histograms;

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       SameProfileWithConflict) {
  ASSERT_TRUE(SetupClients());
  base::HistogramTester histograms;

  AutofillProfile profile0 = CreateAutofillProfile(PROFILE_HOMER);
  AutofillProfile profile1 = CreateAutofillProfile(PROFILE_HOMER);
  profile1.SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER,
                      base::ASCIIToUTF16("1234567890"));

  AddProfile(0, profile0);
  AddProfile(1, profile1);
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

// Flaky on all platform. See crbug.com/971666
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       AddDuplicateProfiles_OneIsVerified) {
  ASSERT_TRUE(SetupClients());
  base::HistogramTester histograms;

  // Create two identical profiles where one of them is verified, additionally.
  AutofillProfile profile0 = autofill::test::GetFullProfile();
  AutofillProfile profile1 =
      autofill::test::GetVerifiedProfile();  // I.e. Full + Verified.
  std::string verified_origin = profile1.origin();

  AddProfile(0, profile0);
  AddProfile(1, profile1);
  // Commit sequentially to make sure there is no race condition.
  SetupSyncOneClientAfterAnother();
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());

  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());
  EXPECT_EQ(verified_origin, GetAllAutoFillProfiles(0)[0]->origin());
  EXPECT_EQ(verified_origin, GetAllAutoFillProfiles(1)[0]->origin());

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

IN_PROC_BROWSER_TEST_F(
    TwoClientAutofillProfileSyncTest,
    AddDuplicateProfiles_OneIsVerified_NonverifiedComesLater) {
  ASSERT_TRUE(SetupClients());
  base::HistogramTester histograms;

  AutofillProfile profile0 = autofill::test::GetFullProfile();
  AutofillProfile profile1 =
      autofill::test::GetVerifiedProfile();  // I.e. Full + Verified.
  std::string verified_origin = profile1.origin();

  // We start by having the verified profile.
  AddProfile(1, profile1);
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());

  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());
  EXPECT_EQ(verified_origin, GetAllAutoFillProfiles(0)[0]->origin());
  EXPECT_EQ(verified_origin, GetAllAutoFillProfiles(1)[0]->origin());

  // Add the same (but non-verified) profile on the other client, afterwards.
  AddProfile(0, profile0);
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());

  // The profiles should de-duplicate via sync on both other client, the
  // verified one should win.
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());

  EXPECT_EQ(verified_origin, GetAllAutoFillProfiles(0)[0]->origin());
  EXPECT_EQ(verified_origin, GetAllAutoFillProfiles(1)[0]->origin());

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

// Tests that a null profile does not get synced across clients.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, AddEmptyProfile) {
  ASSERT_TRUE(SetupSync());

  AddProfile(0, CreateAutofillProfile(PROFILE_NULL));
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(0U, GetAllAutoFillProfiles(0).size());
}

// Tests that adding a profile on one client results in it being added on the
// other client when sync is running.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, AddProfile) {
  ASSERT_TRUE(SetupSync());
  base::HistogramTester histograms;

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));

  // Wait for the sync to happen.
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());

  // Make sure that both clients have one profile.
  EXPECT_EQ(1U, GetProfileCount(0));
  EXPECT_EQ(1U, GetProfileCount(1));

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

// Tests that adding a profile on one client results in it being added on the
// other client when sync gets started.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       AddProfile_BeforeSyncStart) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed";
  base::HistogramTester histograms;

  // Add the new autofill profile before starting sync.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(SetupSync());

  // Wait for the sync to happen.
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());

  // Make sure that both clients have one profile.
  EXPECT_EQ(1U, GetProfileCount(0));
  EXPECT_EQ(1U, GetProfileCount(1));

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

// Tests that adding the same profile on the two clients before sync is started
// results in each client only having one profile after sync is started
// Flaky on all platform, crbug.com/971644.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       DISABLED_ClientsAddSameProfile) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed";
  base::HistogramTester histograms;

  // Add the same profile in the two clients.
  AddProfile(0, CreateUniqueAutofillProfile());
  AddProfile(1, CreateUniqueAutofillProfile());

  // Make sure the GUIDs are different.
  ASSERT_NE(GetAllAutoFillProfiles(0)[0]->guid(),
            GetAllAutoFillProfiles(1)[0]->guid());

  // Commit sequentially to make sure there is no race condition.
  SetupSyncOneClientAfterAnother();
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());

  // Make sure that both clients have one profile.
  EXPECT_EQ(1U, GetProfileCount(0));
  EXPECT_EQ(1U, GetProfileCount(1));

  // Make sure that they have the same GUID.
  EXPECT_EQ(GetAllAutoFillProfiles(0)[0]->guid(),
            GetAllAutoFillProfiles(1)[0]->guid());

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

// Tests that adding multiple profiles to one client results in all of them
// being added to the other client.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       AddMultipleProfilesOnOneClient) {
  ASSERT_TRUE(SetupClients());
  base::HistogramTester histograms;

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(0, CreateAutofillProfile(PROFILE_MARION));
  AddProfile(0, CreateAutofillProfile(PROFILE_FRASIER));
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(3U, GetAllAutoFillProfiles(0).size());

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

// Tests that adding multiple profiles to two client results both clients having
// all profiles.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       AddMultipleProfilesOnTwoClients) {
  ASSERT_TRUE(SetupClients());
  base::HistogramTester histograms;

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(1, CreateAutofillProfile(PROFILE_MARION));
  AddProfile(1, CreateAutofillProfile(PROFILE_FRASIER));
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(3U, GetAllAutoFillProfiles(0).size());

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

// Tests that deleting a profile on one client results in it being deleted on
// the other client.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, DeleteProfile) {
  ASSERT_TRUE(SetupSync());
  base::HistogramTester histograms;

  // Setup the test by making the 2 clients have the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllAutoFillProfiles(0).size());

  // Remove the profile from one client.
  RemoveProfile(1, GetAllAutoFillProfiles(1)[0]->guid());
  EXPECT_TRUE(AutofillProfileChecker(0, 1).Wait());

  // Make sure both clients don't have any profiles.
  EXPECT_EQ(0U, GetAllAutoFillProfiles(0).size());

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 1);
}

// Tests that modifying a profile while syncing results in the other client
// getting the updated profile.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, UpdateFields) {
  ASSERT_TRUE(SetupSync());
  base::HistogramTester histograms;

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

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

// Tests that modifying a profile at the same time one two clients while
// syncing results in the both client having the same profile (doesn't matter
// which one).
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       UpdateConflictingFields) {
  ASSERT_TRUE(SetupSync());
  base::HistogramTester histograms;

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

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

// Tests that modifying a profile at the same time one two clients while
// syncing results in the both client having the same profile (doesn't matter
// which one).
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       UpdateConflictingFieldsDuringInitialMerge) {
  ASSERT_TRUE(SetupClients());
  base::HistogramTester histograms;

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

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

// Tests that modifying a profile at the same time on two clients while
// syncing results in both client having the same profile (doesn't matter which
// one).
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, DeleteAndUpdate) {
  ASSERT_TRUE(SetupSync());
  base::HistogramTester histograms;

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

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 1);
}

// Tests that modifying a profile at the same time on two clients while
// syncing results in a conflict where the update wins. This only works with
// a server that supports a strong consistency model and is hence capable of
// detecting conflicts server-side.
// Flaky (mostly) on ASan/TSan. http://crbug.com/998130
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER)
#define MAYBE_DeleteAndUpdateWithStrongConsistency \
  DISABLED_DeleteAndUpdateWithStrongConsistency
#else
#define MAYBE_DeleteAndUpdateWithStrongConsistency \
  DeleteAndUpdateWithStrongConsistency
#endif
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       MAYBE_DeleteAndUpdateWithStrongConsistency) {
  ASSERT_TRUE(SetupSync());
  base::HistogramTester histograms;
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

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 1);
}

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, MaxLength) {
  ASSERT_TRUE(SetupSync());
  base::HistogramTester histograms;

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

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, ExceedsMaxLength) {
  ASSERT_TRUE(SetupSync());
  base::HistogramTester histograms;

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

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

// Test credit cards don't sync.
IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest, NoCreditCardSync) {
  ASSERT_TRUE(SetupSync());
  base::HistogramTester histograms;

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

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

IN_PROC_BROWSER_TEST_F(TwoClientAutofillProfileSyncTest,
                       E2E_ONLY(TwoClientsAddAutofillProfiles)) {
  ASSERT_TRUE(SetupSync());
  base::HistogramTester histograms;

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

  histograms.ExpectBucketCount("Sync.ModelTypeEntityChange3.AUTOFILL_PROFILE",
                               LOCAL_DELETION, 0);
}

}  // namespace
