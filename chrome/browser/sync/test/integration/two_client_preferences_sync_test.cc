// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/uuid.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync_preferences/common_syncable_prefs_database.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using preferences_helper::BooleanPrefMatches;
using preferences_helper::ChangeBooleanPref;
using preferences_helper::ChangeIntegerPref;
using preferences_helper::ChangeListPref;
using preferences_helper::ChangeStringPref;
using preferences_helper::ClearPref;
using preferences_helper::GetPrefs;
using preferences_helper::GetRegistry;
using testing::Eq;

namespace {

class TwoClientPreferencesSyncTest : public SyncTest {
 public:
  TwoClientPreferencesSyncTest() : SyncTest(TWO_CLIENT) {}

  TwoClientPreferencesSyncTest(const TwoClientPreferencesSyncTest&) = delete;
  TwoClientPreferencesSyncTest& operator=(const TwoClientPreferencesSyncTest&) =
      delete;

  ~TwoClientPreferencesSyncTest() override = default;
};

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, E2E_ENABLED(Sanity)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());
  const std::string new_home_page = base::StringPrintf(
      "https://example.com/%s",
      base::Uuid::GenerateRandomV4().AsLowercaseString().c_str());

  base::HistogramTester histogram_tester;
  ChangeStringPref(0, prefs::kHomePage, new_home_page);
  ASSERT_TRUE(StringPrefMatchChecker(prefs::kHomePage).Wait());
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(new_home_page, GetPrefs(i)->GetString(prefs::kHomePage));
  }

  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.PREFERENCE",
                   syncer::DataTypeEntityChange::kRemoteInitialUpdate));
  // Client 0 may or may not see its own reflection during the test, but at
  // least client 1 should have received one update.
  EXPECT_NE(0, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.PREFERENCE",
                   syncer::DataTypeEntityChange::kRemoteNonInitialUpdate));

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

  base::Value::List urls;
  urls.Append("http://www.google.com/");
  urls.Append("http://www.flickr.com/");
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
  ~TwoClientPreferencesSyncTestWithSelfNotifications() override = default;
};

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTestWithSelfNotifications,
                       E2E_ENABLED(ShouldKeepLocalDataOnTypeMismatch)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  constexpr char string_value[] = "some-string";

  // Client 0 registers a boolean preference, client 1 registers a string.
  GetRegistry(GetProfile(0))
      ->RegisterBooleanPref(sync_preferences::kSyncablePrefForTesting, false,
                            user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  GetRegistry(GetProfile(1))
      ->RegisterStringPref(sync_preferences::kSyncablePrefForTesting, "",
                           user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Set non-default values on both clients.
  ChangeBooleanPref(0, sync_preferences::kSyncablePrefForTesting);
  ChangeStringPref(1, sync_preferences::kSyncablePrefForTesting, string_value);
  ASSERT_THAT(
      GetPrefs(0)->GetBoolean(sync_preferences::kSyncablePrefForTesting),
      Eq(true));
  ASSERT_THAT(GetPrefs(1)->GetString(sync_preferences::kSyncablePrefForTesting),
              Eq(string_value));

  // Start sync and await until they sync mutually.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Verify that neither of the clients got updated, because of type mismatch.
  EXPECT_THAT(
      GetPrefs(0)->GetBoolean(sync_preferences::kSyncablePrefForTesting),
      Eq(true));
  EXPECT_THAT(GetPrefs(1)->GetString(sync_preferences::kSyncablePrefForTesting),
              Eq(string_value));
}

}  // namespace
