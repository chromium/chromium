// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>
#include <tuple>

#include "base/hash/hash.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/test/fake_server_http_post_provider.h"
#include "content/public/test/browser_test.h"
#include "net/base/network_change_notifier.h"

using passwords_helper::AllProfilesContainSamePasswordForms;
using passwords_helper::AllProfilesContainSamePasswordFormsAsVerifier;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetAccountPasswordStoreInterface;
using passwords_helper::GetAllLogins;
using passwords_helper::GetLogins;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetProfilePasswordStoreInterface;
using passwords_helper::GetVerifierPasswordCount;
using passwords_helper::GetVerifierProfilePasswordStoreInterface;
using passwords_helper::RemoveLogins;

using password_manager::InsecureType;
using password_manager::InsecurityMetadata;
using password_manager::IsMuted;
using password_manager::PasswordForm;
using password_manager::TriggerBackendNotification;

using testing::ElementsAre;
using testing::Pointee;
using testing::UnorderedElementsAre;

static const char* kValidPassphrase = "passphrase!";

class TwoClientPasswordsSyncTest : public SyncTest {
 public:
  TwoClientPasswordsSyncTest() : SyncTest(TWO_CLIENT) {}

  ~TwoClientPasswordsSyncTest() override = default;
};

class TwoClientPasswordsSyncTestWithVerifier
    : public TwoClientPasswordsSyncTest {
 public:
  TwoClientPasswordsSyncTestWithVerifier() = default;
  ~TwoClientPasswordsSyncTestWithVerifier() override = default;

  bool UseVerifier() override {
    // TODO(crbug.com/40152785): rewrite tests to not use verifier.
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, E2E_ENABLED(Add)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());

  PasswordForm form = CreateTestPasswordForm(0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(1, GetPasswordCount(1));
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest,
                       E2E_ENABLED(AddInTransportMode)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Sign in on all clients without enabling Sync-the-feature.
  for (int i = 0; i < num_clients(); i++) {
    ASSERT_TRUE(GetClient(i)->SignInPrimaryAccount());
    ASSERT_TRUE(GetClient(i)->AwaitSyncTransportActive());
    ASSERT_FALSE(GetSyncService(i)->IsSyncFeatureEnabled());

    // The PASSWORDS are active only if the signin was explicit.
    if (!switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
      // Opt in. PASSWORDS should become active.
      password_manager::features_util::OptInToAccountStorage(
          GetProfile(i)->GetPrefs(), GetSyncService(i));
    }
    PasswordSyncActiveChecker(GetSyncService(i)).Wait();
  }

  ASSERT_TRUE(
      SamePasswordFormsChecker(PasswordForm::Store::kAccountStore).Wait());

  // Create an account password on the first client.
  PasswordForm form = CreateTestPasswordForm(0);
  GetAccountPasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0, PasswordForm::Store::kAccountStore));

  // The second client should receive the password in its own account store.
  EXPECT_TRUE(
      SamePasswordFormsChecker(PasswordForm::Store::kAccountStore).Wait());
  EXPECT_EQ(1, GetPasswordCount(1, PasswordForm::Store::kAccountStore));
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, E2E_ENABLED(Race)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form0);

  PasswordForm form1 = form0;
  form1.password_value = u"new_password";
  GetProfilePasswordStoreInterface(1)->AddLogin(form1);

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, MergeWithTheMostRecent) {
  // Setup the test to have Form 0 and Form 1 added on both clients. Form 0 is
  // more recent on Client 0, and Form 1 is more recent on Client 1. They should
  // be merged such that recent passwords are chosen.

  base::Time now = base::Time::Now();
  base::Time yesterday = now - base::Days(1);

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
  GetProfilePasswordStoreInterface(0)->AddLogin(form0_recent);
  GetProfilePasswordStoreInterface(0)->AddLogin(form1_old);
  // Enable sync on Client 0 and wait until they are committed.
  ASSERT_TRUE(GetClient(0)->SetupSync()) << "GetClient(0)->SetupSync() failed.";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // Add the passwords to Client 1.
  GetProfilePasswordStoreInterface(1)->AddLogin(form0_old);
  GetProfilePasswordStoreInterface(1)->AddLogin(form1_recent);

  // Enable sync on Client 1 and wait until all passwords are merged.
  ASSERT_TRUE(GetClient(1)->SetupSync()) << "GetClient(1)->SetupSync() failed.";
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());

  // There should be only 2 passwords.
  EXPECT_EQ(2, GetPasswordCount(0));
  // All passwords should be the recent ones.
  for (const std::unique_ptr<PasswordForm>& form :
       GetLogins(GetProfilePasswordStoreInterface(0))) {
    EXPECT_EQ(now, form->date_created);
  }
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest,
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
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTestWithVerifier, Update) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form = CreateTestPasswordForm(0);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form);
  GetProfilePasswordStoreInterface(0)->AddLogin(form);

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());

  form.password_value = u"new_password";
  GetVerifierProfilePasswordStoreInterface()->UpdateLogin(form);
  GetProfilePasswordStoreInterface(1)->UpdateLogin(form);
  ASSERT_EQ(1, GetVerifierPasswordCount());

  // Wait for client 1 to commit and client 0 to receive the update.
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(0).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTestWithVerifier,
                       SharedPasswordMetadataAreSynced) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form = CreateTestPasswordForm(0);
  form.sender_email = u"sender@example.com";
  form.sender_name = u"Sender Name";
  form.sender_profile_image_url = GURL("http://www.sender.com/profile_image");
  form.date_received = form.date_created;
  form.sharing_notification_displayed = true;
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form);
  GetProfilePasswordStoreInterface(0)->AddLogin(form);

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());

  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, AddTwice) {
  // Password store supports adding the same form twice, so this is testing this
  // behaviour.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form = CreateTestPasswordForm(0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(1, GetPasswordCount(1));

  // Update the password and add it again to client 0.
  form.password_value = u"new_password";
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));

  // Wait for client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(1, GetPasswordCount(1));
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTestWithVerifier, Delete) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form0 = CreateTestPasswordForm(0);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form1);
  GetProfilePasswordStoreInterface(0)->AddLogin(form1);

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());

  GetProfilePasswordStoreInterface(1)->RemoveLogin(FROM_HERE, form0);
  GetVerifierProfilePasswordStoreInterface()->RemoveLogin(FROM_HERE, form0);
  ASSERT_EQ(1, GetVerifierPasswordCount());

  // Wait for deletion from client 1 to propagate.
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(0).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest,
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
  ASSERT_TRUE(GetClient(1)->SetupSyncNoWaitForCompletion());
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
  GetProfilePasswordStoreInterface(0)->AddLogin(form0);

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, E2E_ONLY(DeleteTwo)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(
      base::FastHash(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  PasswordForm form1 = CreateTestPasswordForm(
      base::FastHash(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  GetProfilePasswordStoreInterface(0)->AddLogin(form0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form1);

  const int init_password_count = GetPasswordCount(0);

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(init_password_count, GetPasswordCount(1));

  GetProfilePasswordStoreInterface(1)->RemoveLogin(FROM_HERE, form0);

  // Wait for deletion from client 1 to propagate.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(init_password_count - 1, GetPasswordCount(0));

  GetProfilePasswordStoreInterface(1)->RemoveLogin(FROM_HERE, form1);

  // Wait for deletion from client 1 to propagate.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(init_password_count - 2, GetPasswordCount(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTestWithVerifier, DeleteAll) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form0 = CreateTestPasswordForm(0);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form1);
  GetProfilePasswordStoreInterface(0)->AddLogin(form1);
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  RemoveLogins(GetProfilePasswordStoreInterface(1));
  RemoveLogins(GetVerifierProfilePasswordStoreInterface());
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(0).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
  ASSERT_EQ(0, GetVerifierPasswordCount());
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, E2E_ENABLED(Merge)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  GetProfilePasswordStoreInterface(1)->AddLogin(form1);
  PasswordForm form2 = CreateTestPasswordForm(2);
  GetProfilePasswordStoreInterface(1)->AddLogin(form2);

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(3, GetPasswordCount(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, E2E_ONLY(TwoClientAddPass)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  // All profiles should sync same passwords.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait())
      << "Initial password forms did not match for all profiles";
  const int init_password_count = GetPasswordCount(0);

  // Add one new password per profile. A unique form is created for each to
  // prevent them from overwriting each other.
  for (int i = 0; i < num_clients(); ++i) {
    GetProfilePasswordStoreInterface(i)->AddLogin(CreateTestPasswordForm(
        base::RandInt(0, std::numeric_limits<int32_t>::max())));
  }

  // Blocks and waits for password forms in all profiles to match.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());

  // Check that total number of passwords is as expected.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(GetPasswordCount(i), init_password_count + num_clients())
        << "Total password count is wrong.";
  }
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTestWithVerifier,
                       AddImmediatelyAfterDelete) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
  base::HistogramTester histogram_tester;

  PasswordForm form0 = CreateTestPasswordForm(0);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form0);

  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form1 = CreateTestPasswordForm(1);
  GetVerifierProfilePasswordStoreInterface()->UpdateLoginWithPrimaryKey(form1,
                                                                        form0);
  GetProfilePasswordStoreInterface(0)->UpdateLoginWithPrimaryKey(form1, form0);

  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
  // There should be only one deletion. This is to test the bug
  // (crbug.com/1046309) where the USS client was local deletions when receiving
  // remote deletions.
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.PASSWORD",
                   syncer::DataTypeEntityChange::kLocalDeletion));
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest,
                       MergeFormsWithInsecureCredentials) {
  // Setup the test to have Form 0 on Client 0 and Form 1 on Client 1. Both
  // Forms has associated insecure credentials. After sync, both clients should
  // have both forms with their corresponding insecure credentials.

  PasswordForm form0 = CreateTestPasswordForm(0);
  PasswordForm form1 = CreateTestPasswordForm(1);

  form0.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(false),
                          TriggerBackendNotification(false))});
  form1.password_issues.insert(
      {InsecureType::kPhished,
       InsecurityMetadata(base::Time(), IsMuted(false),
                          TriggerBackendNotification(false))});

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add the passwords and security issues to Client 0.
  GetProfilePasswordStoreInterface(0)->AddLogin(form0);

  // Enable sync on Client 0 and wait until they are committed.
  ASSERT_TRUE(GetClient(0)->SetupSync()) << "GetClient(0)->SetupSync() failed.";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // Add the passwords and security issues to Client 1.
  GetProfilePasswordStoreInterface(1)->AddLogin(form1);

  // Enable sync on Client 1 and wait until all passwords are merged.
  ASSERT_TRUE(GetClient(1)->SetupSync()) << "GetClient(1)->SetupSync() failed.";
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());

  EXPECT_THAT(GetAllLogins(GetProfilePasswordStoreInterface(0)),
              UnorderedElementsAre(
                  Pointee(password_manager::HasPrimaryKeyAndEquals(form0)),
                  Pointee(password_manager::HasPrimaryKeyAndEquals(form1))));
  EXPECT_THAT(GetAllLogins(GetProfilePasswordStoreInterface(1)),
              UnorderedElementsAre(
                  Pointee(password_manager::HasPrimaryKeyAndEquals(form0)),
                  Pointee(password_manager::HasPrimaryKeyAndEquals(form1))));
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest,
                       AddFormWithInsecureCredentials) {
  // Tests that newly added form with security issues is successfully synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form = CreateTestPasswordForm(0);
  form.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(false),
                          TriggerBackendNotification(false))});
  form.password_issues.insert(
      {InsecureType::kPhished,
       InsecurityMetadata(base::Time(), IsMuted(false),
                          TriggerBackendNotification(false))});

  // Add the form and security issues to Client 0.
  GetProfilePasswordStoreInterface(0)->AddLogin(form);

  // Wait until Client 1 picks up changes.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  EXPECT_THAT(
      GetAllLogins(GetProfilePasswordStoreInterface(1)),
      ElementsAre(Pointee(password_manager::HasPrimaryKeyAndEquals(form))));
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, RemoveInsecureCredentialss) {
  // Tests that removing security issues are successfully synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(0);
  PasswordForm form1 = CreateTestPasswordForm(1);

  form0.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(false),
                          TriggerBackendNotification(false))});
  form1.password_issues.insert(
      {InsecureType::kPhished,
       InsecurityMetadata(base::Time(), IsMuted(false),
                          TriggerBackendNotification(false))});

  // Add the form and security issues to Client 0.
  GetProfilePasswordStoreInterface(0)->AddLogin(form0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form1);

  // Wait until Client 1 picks up changes.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  EXPECT_THAT(GetAllLogins(GetProfilePasswordStoreInterface(1)),
              UnorderedElementsAre(
                  Pointee(password_manager::HasPrimaryKeyAndEquals(form0)),
                  Pointee(password_manager::HasPrimaryKeyAndEquals(form1))));

  // Remove security issues on Client 1.
  form0.password_issues.clear();
  GetProfilePasswordStoreInterface(1)->UpdateLogin(form0);

  // Wait until Client 0 picks up changes.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  EXPECT_THAT(GetAllLogins(GetProfilePasswordStoreInterface(1)),
              UnorderedElementsAre(
                  Pointee(password_manager::HasPrimaryKeyAndEquals(form0)),
                  Pointee(password_manager::HasPrimaryKeyAndEquals(form1))));
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest,
                       InsecureCredentialUpdateMute) {
  // Tests that updating security issues are successfully synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form = CreateTestPasswordForm(0);
  form.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(false),
                          TriggerBackendNotification(false))});

  // Add the form and security issue to Client 0.
  GetProfilePasswordStoreInterface(0)->AddLogin(form);

  // Wait until Client 1 picks up changes.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());

  // Update is_muted field on Client 0.
  form.password_issues.at(InsecureType::kLeaked).is_muted = IsMuted(true);
  GetProfilePasswordStoreInterface(0)->UpdateLogin(form);

  // Wait until Client 1 picks up changes.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  EXPECT_THAT(
      GetAllLogins(GetProfilePasswordStoreInterface(1)),
      ElementsAre(Pointee(password_manager::HasPrimaryKeyAndEquals(form))));
}

// Regression test for crbug.com/1346576.
IN_PROC_BROWSER_TEST_F(
    TwoClientPasswordsSyncTest,
    MatchingDeletionsConflictDoesNotInvokeTrimmingEntitySpecifics) {
  // Add a password and wait until it is synced on both clients.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  PasswordForm form = CreateTestPasswordForm(0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(GetPasswordCount(0), 1);

  // Simulate going offline on both clients.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  // Remove the password from both clients to simulate a conflict with matching
  // remote and local deletion after Client 1 comes back online.
  GetProfilePasswordStoreInterface(0)->RemoveLogin(FROM_HERE, form);
  GetProfilePasswordStoreInterface(1)->RemoveLogin(FROM_HERE, form);

  // Simulate going online again.
  fake_server::FakeServerHttpPostProvider::EnableNetwork();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);

  // Checks that the client does not crash due to trimming entity specifics for
  // caching for a deleted entity (without a password field).
  ASSERT_TRUE(AwaitQuiescence());
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest,
                       SyncPasswordNotesBetweenDevices) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  // Add a password with note to Client 0.
  PasswordForm form = CreateTestPasswordForm(0);
  form.notes.emplace_back(
      /*unique_display_name=*/u"My Phone Pin", /*value=*/u"123456",
      /*date_created=*/base::Time::Now(), /*hide_by_default=*/true);
  GetProfilePasswordStoreInterface(0)->AddLogin(form);

  // Wait until Client 1 picks up changes.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  EXPECT_THAT(
      GetAllLogins(GetProfilePasswordStoreInterface(1)),
      ElementsAre(Pointee(password_manager::HasPrimaryKeyAndEquals(form))));

  // Update the note in Client 1.
  form.notes[0].value = u"78910";
  GetProfilePasswordStoreInterface(1)->UpdateLogin(form);

  // Wait until Client 0 picks up changes.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  EXPECT_THAT(
      GetAllLogins(GetProfilePasswordStoreInterface(0)),
      ElementsAre(Pointee(password_manager::HasPrimaryKeyAndEquals(form))));

  // Remove all notes on Client 0.
  form.notes.clear();
  GetProfilePasswordStoreInterface(0)->UpdateLogin(form);

  // Wait until Client 1 picks up changes.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  EXPECT_THAT(
      GetAllLogins(GetProfilePasswordStoreInterface(1)),
      ElementsAre(Pointee(password_manager::HasPrimaryKeyAndEquals(form))));
}

// This tests the  logic for reading and writing the notes backup blob when
// notes are empty.
IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest,
                       SyncPasswordWithEmptyNotesBetweenDevices) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  // Add a password with note to Client 0.
  PasswordForm form = CreateTestPasswordForm(0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form);

  // Wait until Client 1 picks up changes.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  EXPECT_THAT(
      GetAllLogins(GetProfilePasswordStoreInterface(1)),
      ElementsAre(Pointee(password_manager::HasPrimaryKeyAndEquals(form))));

  // Update the password in Client 1.
  form.password_value = u"new_password";
  GetProfilePasswordStoreInterface(1)->UpdateLogin(form);

  // Wait until Client 0 picks up changes.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  EXPECT_THAT(
      GetAllLogins(GetProfilePasswordStoreInterface(0)),
      ElementsAre(Pointee(password_manager::HasPrimaryKeyAndEquals(form))));
}
