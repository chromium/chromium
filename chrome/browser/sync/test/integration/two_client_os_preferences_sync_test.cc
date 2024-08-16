// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/prefs/pref_service.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using preferences_helper::ChangeStringPref;
using preferences_helper::ClearPref;
using preferences_helper::GetPrefs;

namespace {

class TwoClientOsPreferencesSyncTest : public SyncTest {
 public:
  TwoClientOsPreferencesSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientOsPreferencesSyncTest() override = default;

  // Needed for AwaitQuiescence().
  bool TestUsesSelfNotifications() override { return true; }
};

IN_PROC_BROWSER_TEST_F(TwoClientOsPreferencesSyncTest, E2E_ENABLED(Sanity)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  // Wait until sync settles before we override the prefs below.
  ASSERT_TRUE(AwaitQuiescence());

  // Shelf alignment is a Chrome OS only preference.
  ASSERT_TRUE(StringPrefMatchChecker(ash::prefs::kShelfAlignment).Wait());

  base::HistogramTester histogram_tester;
  ChangeStringPref(0, ash::prefs::kShelfAlignment, ash::kShelfAlignmentRight);
  ASSERT_TRUE(StringPrefMatchChecker(ash::prefs::kShelfAlignment).Wait());
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(ash::kShelfAlignmentRight,
              GetPrefs(i)->GetString(ash::prefs::kShelfAlignment));
  }

  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.OS_PREFERENCE",
                   syncer::DataTypeEntityChange::kRemoteInitialUpdate));
  // Client 0 may or may not see its own reflection during the test, but at
  // least client 1 should have received one update.
  EXPECT_NE(0, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.OS_PREFERENCE",
                   syncer::DataTypeEntityChange::kRemoteNonInitialUpdate));
  EXPECT_NE(
      0U,
      histogram_tester
          .GetAllSamples(
              "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.OS_PREFERENCE")
          .size());
  EXPECT_NE(
      0U, histogram_tester
              .GetAllSamples("Sync.NonReflectionUpdateFreshnessPossiblySkewed2")
              .size());
}

IN_PROC_BROWSER_TEST_F(TwoClientOsPreferencesSyncTest,
                       E2E_ENABLED(Bidirectional)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(StringPrefMatchChecker(ash::prefs::kShelfAlignment).Wait());

  ChangeStringPref(0, ash::prefs::kShelfAlignment, ash::kShelfAlignmentRight);
  ASSERT_TRUE(StringPrefMatchChecker(ash::prefs::kShelfAlignment).Wait());
  EXPECT_EQ(ash::kShelfAlignmentRight,
            GetPrefs(0)->GetString(ash::prefs::kShelfAlignment));

  ChangeStringPref(1, ash::prefs::kShelfAlignment, ash::kShelfAlignmentLeft);
  ASSERT_TRUE(StringPrefMatchChecker(ash::prefs::kShelfAlignment).Wait());
  EXPECT_EQ(ash::kShelfAlignmentLeft,
            GetPrefs(0)->GetString(ash::prefs::kShelfAlignment));
}

IN_PROC_BROWSER_TEST_F(TwoClientOsPreferencesSyncTest, E2E_ENABLED(ClearPref)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync());
  ChangeStringPref(0, ash::prefs::kShelfAlignment, ash::kShelfAlignmentRight);
  ASSERT_TRUE(StringPrefMatchChecker(ash::prefs::kShelfAlignment).Wait());

  ClearPref(0, ash::prefs::kShelfAlignment);

  ASSERT_TRUE(ClearedPrefMatchChecker(ash::prefs::kShelfAlignment).Wait());
}

// OS Settings syncing even when browser sync is disabled.
IN_PROC_BROWSER_TEST_F(TwoClientOsPreferencesSyncTest, BrowserSyncDisabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  for (int i = 0; i < num_clients(); ++i) {
    // Disable all browser types.
    GetSyncService(i)->GetUserSettings()->SetSelectedTypes(
        false, syncer::UserSelectableTypeSet());
    ASSERT_TRUE(GetClient(i)->AwaitSyncSetupCompletion());
  }

  ChangeStringPref(0, ash::prefs::kShelfAlignment, ash::kShelfAlignmentRight);
  EXPECT_TRUE(StringPrefMatchChecker(ash::prefs::kShelfAlignment).Wait());
}

}  // namespace
