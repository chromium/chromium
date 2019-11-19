// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using preferences_helper::BooleanPrefMatches;
using preferences_helper::BuildPrefStoreFromPrefsFile;
using preferences_helper::ChangeBooleanPref;
using preferences_helper::ChangeIntegerPref;
using preferences_helper::ChangeListPref;
using preferences_helper::ChangeStringPref;
using preferences_helper::ClearPref;
using preferences_helper::GetPrefs;
using preferences_helper::GetRegistry;
using testing::Eq;
using user_prefs::PrefRegistrySyncable;

namespace {

class TwoClientPreferencesSyncTest : public SyncTest {
 public:
  TwoClientPreferencesSyncTest() : SyncTest(TWO_CLIENT) {}

  ~TwoClientPreferencesSyncTest() override {}

  // Needed for AwaitQuiescence().
  bool TestUsesSelfNotifications() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientPreferencesSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, E2E_ENABLED(Sanity)) {
  ResetSyncForPrimaryAccount();
  DisableVerifier();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  // Wait until sync settles before we override the prefs below.
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());
  const std::string new_home_page = base::StringPrintf(
      "https://example.com/%s", base::GenerateGUID().c_str());

  base::HistogramTester histogram_tester;
  ChangeStringPref(0, prefs::kHomePage, new_home_page);
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(new_home_page, GetPrefs(i)->GetString(prefs::kHomePage));
  }

  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Sync.ModelTypeEntityChange3.PREFERENCE",
                   /*REMOTE_INITIAL_UPDATE=*/5));
  // Client 0 may or may not see its own reflection during the test, but at
  // least client 1 should have received one update.
  EXPECT_NE(0, histogram_tester.GetBucketCount(
                   "Sync.ModelTypeEntityChange3.PREFERENCE",
                   /*REMOTE_NON_INITIAL_UPDATE=*/4));

  EXPECT_NE(
      0U, histogram_tester
              .GetAllSamples(
                  "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.PREFERENCE")
              .size());
  EXPECT_NE(
      0U, histogram_tester
              .GetAllSamples("Sync.NonReflectionUpdateFreshnessPossiblySkewed2")
              .size());
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, E2E_ENABLED(BooleanPref)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BooleanPrefMatchChecker(prefs::kHomePageIsNewTabPage).Wait());

  ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ASSERT_TRUE(BooleanPrefMatchChecker(prefs::kHomePageIsNewTabPage).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       E2E_ENABLED(Bidirectional)) {
  ResetSyncForPrimaryAccount();
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

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       E2E_ENABLED(UnsyncableBooleanPref)) {
  ResetSyncForPrimaryAccount();
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

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, E2E_ENABLED(StringPref)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());

  ChangeStringPref(0, prefs::kHomePage, "http://news.google.com");
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, E2E_ENABLED(ClearPref)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ChangeStringPref(0, prefs::kHomePage, "http://news.google.com");
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());

  ClearPref(0, prefs::kHomePage);

  ASSERT_TRUE(ClearedPrefMatchChecker(prefs::kHomePage).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       E2E_ENABLED(ComplexPrefs)) {
  ResetSyncForPrimaryAccount();
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

// The following tests use lower-level mechanisms to wait for sync cycle
// completions. Those only work reliably with self notifications turned on.
class TwoClientPreferencesSyncTestWithSelfNotifications : public SyncTest {
 public:
  TwoClientPreferencesSyncTestWithSelfNotifications() : SyncTest(TWO_CLIENT) {}

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

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTestWithSelfNotifications,
                       E2E_ENABLED(ShouldKeepLocalDataOnTypeMismatch)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  constexpr char pref_name[] = "testing.my-test-preference";
  constexpr char string_value[] = "some-string";

  // Client 0 registers a boolean preference, client 1 registers a string.
  GetRegistry(GetProfile(0))
      ->RegisterBooleanPref(pref_name, false,
                            user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  GetRegistry(GetProfile(1))
      ->RegisterStringPref(pref_name, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Set non-default values on both clients.
  ChangeBooleanPref(0, pref_name);
  ChangeStringPref(1, pref_name, string_value);
  ASSERT_THAT(GetPrefs(0)->GetBoolean(pref_name), Eq(true));
  ASSERT_THAT(GetPrefs(1)->GetString(pref_name), Eq(string_value));

  // Start sync and await until they sync mutually.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Verify that neither of the clients got updated, because of type mismatch.
  EXPECT_THAT(GetPrefs(0)->GetBoolean(pref_name), Eq(true));
  EXPECT_THAT(GetPrefs(1)->GetString(pref_name), Eq(string_value));

  // Only one of the two clients sees the mismatch, the one sync-ing last.
  histogram_tester.ExpectTotalCount("Sync.Preferences.RemotePrefTypeMismatch",
                                    1);
}

// Verifies that priority synced preferences and regular sycned preferences are
// kept separate.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTestWithSelfNotifications,
                       E2E_ENABLED(ShouldIsolatePriorityPreferences)) {
  ResetSyncForPrimaryAccount();
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

}  // namespace
