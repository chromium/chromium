// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

using preferences_helper::BooleanPrefMatches;
using preferences_helper::BuildPrefStoreFromPrefsFile;
using preferences_helper::ChangeBooleanPref;
using preferences_helper::ChangeIntegerPref;
using preferences_helper::ChangeListPref;
using preferences_helper::ChangeStringPref;
using preferences_helper::GetPrefs;
using preferences_helper::GetRegistry;
using testing::Eq;
using user_prefs::PrefRegistrySyncable;

namespace {

class TwoClientPreferencesSyncTest : public FeatureToggler, public SyncTest {
 public:
  TwoClientPreferencesSyncTest()
      : FeatureToggler(switches::kSyncPseudoUSSPreferences),
        SyncTest(TWO_CLIENT) {}
  ~TwoClientPreferencesSyncTest() override {}

  bool TestUsesSelfNotifications() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientPreferencesSyncTest);
};

IN_PROC_BROWSER_TEST_P(TwoClientPreferencesSyncTest, E2E_ENABLED(Sanity)) {
  DisableVerifier();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());
  const std::string new_home_page = base::StringPrintf(
      "https://example.com/%s", base::GenerateGUID().c_str());
  ChangeStringPref(0, prefs::kHomePage, new_home_page);
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(new_home_page, GetPrefs(i)->GetString(prefs::kHomePage));
  }
}

IN_PROC_BROWSER_TEST_P(TwoClientPreferencesSyncTest, E2E_ENABLED(BooleanPref)) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BooleanPrefMatchChecker(prefs::kHomePageIsNewTabPage).Wait());

  ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ASSERT_TRUE(BooleanPrefMatchChecker(prefs::kHomePageIsNewTabPage).Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientPreferencesSyncTest,
                       E2E_ENABLED(Bidirectional)) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());

  ChangeStringPref(0, prefs::kHomePage, "http://www.google.com/0");
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());
  EXPECT_EQ("http://www.google.com/0",
            GetPrefs(0)->GetString(prefs::kHomePage));

  ChangeStringPref(1, prefs::kHomePage, "http://www.google.com/1");
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());
  EXPECT_EQ("http://www.google.com/1",
            GetPrefs(0)->GetString(prefs::kHomePage));
}

IN_PROC_BROWSER_TEST_P(TwoClientPreferencesSyncTest,
                       E2E_ENABLED(UnsyncableBooleanPref)) {
  ASSERT_TRUE(SetupSync());
  DisableVerifier();
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());
  ASSERT_TRUE(BooleanPrefMatchChecker(prefs::kDisableScreenshots).Wait());

  // This pref is not syncable.
  ChangeBooleanPref(0, prefs::kDisableScreenshots);

  // This pref is syncable.
  ChangeStringPref(0, prefs::kHomePage, "http://news.google.com");

  // Wait until the syncable pref is synced, then expect that the non-syncable
  // one is still out of sync.
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());
  ASSERT_FALSE(BooleanPrefMatches(prefs::kDisableScreenshots));
}

IN_PROC_BROWSER_TEST_P(TwoClientPreferencesSyncTest, E2E_ENABLED(StringPref)) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());

  ChangeStringPref(0, prefs::kHomePage, "http://news.google.com");
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientPreferencesSyncTest,
                       E2E_ENABLED(ComplexPrefs)) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(IntegerPrefMatchChecker(prefs::kRestoreOnStartup).Wait());
  ASSERT_TRUE(ListPrefMatchChecker(prefs::kURLsToRestoreOnStartup).Wait());

  ChangeIntegerPref(0, prefs::kRestoreOnStartup, 0);
  ASSERT_TRUE(IntegerPrefMatchChecker(prefs::kRestoreOnStartup).Wait());

  base::ListValue urls;
  urls.AppendString("http://www.google.com/");
  urls.AppendString("http://www.flickr.com/");
  ChangeIntegerPref(0, prefs::kRestoreOnStartup, 4);
  ChangeListPref(0, prefs::kURLsToRestoreOnStartup, urls);
  ASSERT_TRUE(IntegerPrefMatchChecker(prefs::kRestoreOnStartup).Wait());
  ASSERT_TRUE(ListPrefMatchChecker(prefs::kURLsToRestoreOnStartup).Wait());
}

// Disabled due to flakiness on Chrome OS: https://crbug.com/873902.
#if defined(OS_CHROMEOS)
#define MAYBE_SingleClientEnabledEncryptionBothChanged \
  DISABLED_SingleClientEnabledEncryptionBothChanged
#else
#define MAYBE_SingleClientEnabledEncryptionBothChanged \
  SingleClientEnabledEncryptionBothChanged
#endif
IN_PROC_BROWSER_TEST_P(
    TwoClientPreferencesSyncTest,
    E2E_ENABLED(MAYBE_SingleClientEnabledEncryptionBothChanged)) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BooleanPrefMatchChecker(prefs::kHomePageIsNewTabPage).Wait());
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());

  ASSERT_TRUE(EnableEncryption(0));
  ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ChangeStringPref(1, prefs::kHomePage, "http://www.google.com/1");
  ASSERT_TRUE(AwaitEncryptionComplete(0));
  ASSERT_TRUE(AwaitEncryptionComplete(1));
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());
  ASSERT_TRUE(BooleanPrefMatchChecker(prefs::kHomePageIsNewTabPage).Wait());
}

IN_PROC_BROWSER_TEST_P(
    TwoClientPreferencesSyncTest,
    E2E_ENABLED(BothClientsEnabledEncryptionAndChangedMultipleTimes)) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BooleanPrefMatchChecker(prefs::kHomePageIsNewTabPage).Wait());

  ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(EnableEncryption(1));
  ASSERT_TRUE(BooleanPrefMatchChecker(prefs::kHomePageIsNewTabPage).Wait());

  ASSERT_TRUE(BooleanPrefMatchChecker(prefs::kShowHomeButton).Wait());
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(BooleanPrefMatchChecker(prefs::kShowHomeButton).Wait());
}

// The following tests use lower-level mechanisms to wait for sync cycle
// completions. Those only work reliably with self notifications turned on.
class TwoClientPreferencesSyncTestWithSelfNotifications : public FeatureToggler,
                                                          public SyncTest {
 public:
  TwoClientPreferencesSyncTestWithSelfNotifications()
      : FeatureToggler(switches::kSyncPseudoUSSPreferences),
        SyncTest(TWO_CLIENT) {}
  ~TwoClientPreferencesSyncTestWithSelfNotifications() override {}

  void SetUp() override {
    // If verifiers are enabled, ChangeBooleanPref() and similar methods will
    // apply changes to both the specified client and the verifier profile.
    // These tests should only apply changes in one client.
    DisableVerifier();
    SyncTest::SetUp();
  }

  bool TestUsesSelfNotifications() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientPreferencesSyncTestWithSelfNotifications);
};

// Tests that late registered prefs are kept in sync with other clients.
IN_PROC_BROWSER_TEST_P(TwoClientPreferencesSyncTestWithSelfNotifications,
                       E2E_ENABLED(LateRegisteredPrefsShouldSync)) {
  // client0 has the pref registered before sync and is modifying a pref before
  // that pref got registered with client1 (but after client1 started syncing).
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  constexpr char pref_name[] = "testing.my-test-preference";
  GetRegistry(GetProfile(0))
      ->RegisterBooleanPref(pref_name, false,
                            user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  GetRegistry(GetProfile(1))
      ->WhitelistLateRegistrationPrefForSync("testing.my-test-preference");

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_THAT(GetPrefs(0)->GetBoolean(pref_name), Eq(false));
  ChangeBooleanPref(0, pref_name);
  ASSERT_THAT(GetPrefs(0)->GetBoolean(pref_name), Eq(true));
  GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1));

  // Now register the pref and verify it's up-to-date.
  GetRegistry(GetProfile(1))
      ->RegisterBooleanPref(pref_name, false,
                            user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  EXPECT_THAT(GetPrefs(1)->GetBoolean(pref_name), Eq(true));

  // Make sure that subsequent changes are synced.
  ChangeBooleanPref(0, pref_name);
  ASSERT_THAT(GetPrefs(0)->GetBoolean(pref_name), Eq(false));
  EXPECT_TRUE(BooleanPrefMatchChecker(pref_name).Wait());
  EXPECT_THAT(GetPrefs(1)->GetBoolean(pref_name), Eq(false));

  // Make sure that subsequent changes are synced.
  ChangeBooleanPref(1, pref_name);
  ASSERT_THAT(GetPrefs(1)->GetBoolean(pref_name), Eq(true));
  EXPECT_TRUE(BooleanPrefMatchChecker(pref_name).Wait());
  EXPECT_THAT(GetPrefs(0)->GetBoolean(pref_name), Eq(true));
}

IN_PROC_BROWSER_TEST_P(TwoClientPreferencesSyncTestWithSelfNotifications,
                       E2E_ENABLED(ShouldKeepLocalDataOnTypeMismatch)) {
  // Client 1 has type-conflicting data in it's pref file. Verify that incoming
  // values from sync of other type do not modify the local state.
  SetPreexistingPreferencesFileContents(
      1, "{\"testing\":{\"my-test-preference\": \"some-string\"}}");
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  constexpr char pref_name[] = "testing.my-test-preference";
  GetRegistry(GetProfile(0))
      ->RegisterBooleanPref(pref_name, false,
                            user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  GetRegistry(GetProfile(1))
      ->WhitelistLateRegistrationPrefForSync("testing.my-test-preference");
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ChangeBooleanPref(0, pref_name);
  ASSERT_THAT(GetPrefs(0)->GetBoolean(pref_name), Eq(true));
  GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1));

  // Verify the value got not stored at client1 (because of type mismatch).
  scoped_refptr<PrefStore> pref_store =
      BuildPrefStoreFromPrefsFile(GetProfile(1));
  const base::Value* result;
  ASSERT_TRUE(pref_store->GetValue("testing.my-test-preference", &result));
  EXPECT_THAT(result->GetString(), Eq("some-string"));

  // Verify reads at client1 get served the default value.
  GetRegistry(GetProfile(1))
      ->RegisterBooleanPref(pref_name, false,
                            user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  EXPECT_THAT(GetPrefs(1)->GetBoolean(pref_name), Eq(false));
}

// Verifies that priority synced preferences and regular sycned preferences are
// kept separate.
IN_PROC_BROWSER_TEST_P(TwoClientPreferencesSyncTestWithSelfNotifications,
                       E2E_ENABLED(ShouldIsolatePriorityPreferences)) {
  // Register a pref as priority with client0 and regular synced with client1.
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  constexpr char pref_name[] = "testing.my-test-preference";
  GetRegistry(GetProfile(0))
      ->RegisterStringPref(
          pref_name, "",
          user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  GetRegistry(GetProfile(1))
      ->RegisterStringPref(pref_name, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ChangeStringPref(0, pref_name, "priority value");
  GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1));
  EXPECT_THAT(GetPrefs(0)->GetString(pref_name), Eq("priority value"));
  EXPECT_THAT(GetPrefs(1)->GetString(pref_name), Eq(""));

  ChangeStringPref(1, pref_name, "non-priority value");
  GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0));
  EXPECT_THAT(GetPrefs(0)->GetString(pref_name), Eq("priority value"));
  EXPECT_THAT(GetPrefs(1)->GetString(pref_name), Eq("non-priority value"));
}

INSTANTIATE_TEST_CASE_P(USS,
                        TwoClientPreferencesSyncTest,
                        ::testing::Values(false, true));

INSTANTIATE_TEST_CASE_P(USS,
                        TwoClientPreferencesSyncTestWithSelfNotifications,
                        ::testing::Values(false, true));

}  // namespace
