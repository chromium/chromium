// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>
#include <tuple>

#include "base/guid.h"
#include "base/hash/hash.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/model_safe_worker.h"

using passwords_helper::AddLogin;
using passwords_helper::AllProfilesContainSamePasswordForms;
using passwords_helper::AllProfilesContainSamePasswordFormsAsVerifier;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetLogins;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;
using passwords_helper::GetVerifierPasswordCount;
using passwords_helper::GetVerifierPasswordStore;
using passwords_helper::RemoveLogin;
using passwords_helper::RemoveLogins;
using passwords_helper::UpdateLogin;
using passwords_helper::UpdateLoginWithPrimaryKey;

using autofill::PasswordForm;

static const char* kValidPassphrase = "passphrase!";

class TwoClientPasswordsSyncTest : public FeatureToggler, public SyncTest {
 public:
  TwoClientPasswordsSyncTest()
      : FeatureToggler(switches::kSyncUSSPasswords), SyncTest(TWO_CLIENT) {}

  ~TwoClientPasswordsSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientPasswordsSyncTest);
};

IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, E2E_ENABLED(Add)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(1, GetPasswordCount(1));
}

IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, E2E_ENABLED(Race)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form0);

  PasswordForm form1 = form0;
  form1.password_value = base::ASCIIToUTF16("new_password");
  AddLogin(GetPasswordStore(1), form1);

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, MergeWithTheMostRecent) {
  // Setup the test to have Form 0 and Form 1 added on both clients. Form 0 is
  // more recent on Client 0, and Form 1 is more recent on Client 1. They should
  // be merged such that recent passwords are chosen.

  base::Time now = base::Time::Now();
  base::Time yesterday = now - base::TimeDelta::FromDays(1);

  PasswordForm form0_recent = CreateTestPasswordForm(0);
  form0_recent.date_created = now;
  PasswordForm form0_old = CreateTestPasswordForm(0);
  form0_old.date_created = yesterday;

  PasswordForm form1_recent = CreateTestPasswordForm(1);
  form1_recent.date_created = now;
  PasswordForm form1_old = CreateTestPasswordForm(1);
  form1_old.date_created = yesterday;

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add the passwords to Client 0.
  AddLogin(GetPasswordStore(0), form0_recent);
  AddLogin(GetPasswordStore(0), form1_old);
  // Enable sync on Client 0 and wait until they are committed.
  ASSERT_TRUE(GetClient(0)->SetupSync()) << "GetClient(0)->SetupSync() failed.";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // Add the passwords to Client 1.
  AddLogin(GetPasswordStore(1), form0_old);
  AddLogin(GetPasswordStore(1), form1_recent);

  // Enable sync on Client 1 and wait until all passwords are merged.
  ASSERT_TRUE(GetClient(1)->SetupSync()) << "GetClient(1)->SetupSync() failed.";
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());

  // There should be only 2 passwords.
  EXPECT_EQ(2, GetPasswordCount(0));
  // All passwords should be the recent ones.
  for (const std::unique_ptr<PasswordForm>& form :
       GetLogins(GetPasswordStore(0))) {
    EXPECT_EQ(now, form->date_created);
  }
}

IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest,
                       E2E_ENABLED(SetPassphraseAndAddPassword)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase(
      kValidPassphrase);
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());

  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(1)).Wait());
  ASSERT_TRUE(GetSyncService(1)->GetUserSettings()->SetDecryptionPassphrase(
      kValidPassphrase));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(1)).Wait());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, Update) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  AddLogin(GetPasswordStore(0), form);

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());

  form.password_value = base::ASCIIToUTF16("new_password");
  UpdateLogin(GetVerifierPasswordStore(), form);
  UpdateLogin(GetPasswordStore(1), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());

  // Wait for client 1 to commit and client 0 to receive the update.
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(0).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, AddTwice) {
  // Password store supports adding the same form twice, so this is testing this
  // behaviour.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(1, GetPasswordCount(1));

  // Update the password and add it again to client 0.
  form.password_value = base::ASCIIToUTF16("new_password");
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  // Wait for client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(1, GetPasswordCount(1));
}

IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, Delete) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form0);
  AddLogin(GetPasswordStore(0), form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  AddLogin(GetVerifierPasswordStore(), form1);
  AddLogin(GetPasswordStore(0), form1);

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());

  RemoveLogin(GetPasswordStore(1), form0);
  RemoveLogin(GetVerifierPasswordStore(), form0);
  ASSERT_EQ(1, GetVerifierPasswordCount());

  // Wait for deletion from client 1 to propagate.
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(0).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest,
                       SetPassphraseAndThenSetupSync) {
  ASSERT_TRUE(SetupClients());

  ASSERT_TRUE(GetClient(0)->SetupSync());
  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase(
      kValidPassphrase);
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());
  // Wait for the client to commit the updates.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // When client 1 hits a passphrase required state, we can infer that
  // client 0's passphrase has been committed. to the server.
  ASSERT_TRUE(GetClient(1)->SetupSyncNoWaitForCompletion(
      GetRegisteredSelectableTypes(1)));
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(1)).Wait());

  // Get client 1 out of the passphrase required state.
  ASSERT_TRUE(GetSyncService(1)->GetUserSettings()->SetDecryptionPassphrase(
      kValidPassphrase));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(1)).Wait());

  // We must mark the setup complete now, since we just entered the passphrase
  // and the previous SetupSync() call failed.
  GetClient(1)->FinishSyncSetup();

  // Move around some passwords to make sure it's all working.
  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form0);

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, E2E_ONLY(DeleteTwo)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(base::Hash(base::GenerateGUID()));
  PasswordForm form1 = CreateTestPasswordForm(base::Hash(base::GenerateGUID()));
  AddLogin(GetPasswordStore(0), form0);
  AddLogin(GetPasswordStore(0), form1);

  const int init_password_count = GetPasswordCount(0);

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(init_password_count, GetPasswordCount(1));

  RemoveLogin(GetPasswordStore(1), form0);

  // Wait for deletion from client 1 to propagate.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(init_password_count - 1, GetPasswordCount(0));

  RemoveLogin(GetPasswordStore(1), form1);

  // Wait for deletion from client 1 to propagate.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(init_password_count - 2, GetPasswordCount(0));
}

IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, DeleteAll) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form0);
  AddLogin(GetPasswordStore(0), form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  AddLogin(GetVerifierPasswordStore(), form1);
  AddLogin(GetPasswordStore(0), form1);
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  RemoveLogins(GetPasswordStore(1));
  RemoveLogins(GetVerifierPasswordStore());
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(0).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
  ASSERT_EQ(0, GetVerifierPasswordCount());
}

IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, E2E_ENABLED(Merge)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  AddLogin(GetPasswordStore(1), form1);
  PasswordForm form2 = CreateTestPasswordForm(2);
  AddLogin(GetPasswordStore(1), form2);

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(3, GetPasswordCount(0));
}

IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, E2E_ONLY(TwoClientAddPass)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) <<  "SetupSync() failed.";
  // All profiles should sync same passwords.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait())
      << "Initial password forms did not match for all profiles";
  const int init_password_count = GetPasswordCount(0);

  // Add one new password per profile. A unique form is created for each to
  // prevent them from overwriting each other.
  for (int i = 0; i < num_clients(); ++i) {
    AddLogin(GetPasswordStore(i), CreateTestPasswordForm(base::RandInt(
                                      0, std::numeric_limits<int32_t>::max())));
  }

  // Blocks and waits for password forms in all profiles to match.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());

  // Check that total number of passwords is as expected.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(GetPasswordCount(i), init_password_count + num_clients()) <<
        "Total password count is wrong.";
  }
}

IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, AddImmediatelyAfterDelete) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form0);
  AddLogin(GetPasswordStore(0), form0);

  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form1 = CreateTestPasswordForm(1);
  UpdateLoginWithPrimaryKey(GetVerifierPasswordStore(), form1, form0);
  UpdateLoginWithPrimaryKey(GetPasswordStore(0), form1, form0);

  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

INSTANTIATE_TEST_SUITE_P(USS,
                         TwoClientPasswordsSyncTest,
                         ::testing::Values(false, true));
