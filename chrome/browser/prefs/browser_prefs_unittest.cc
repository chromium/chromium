// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_prefs.h"

#include "build/build_config.h"
#include "components/sync/base/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kSyncRequested[] = "sync.requested";

class BrowserPrefsTest : public testing::Test {
 protected:
  BrowserPrefsTest() { RegisterUserProfilePrefs(prefs_.registry()); }

  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(BrowserPrefsTest, MigrateObsoleteProfilePrefSyncRequestedDefaultValue) {
  MigrateObsoleteProfilePrefs(&prefs_);
  EXPECT_EQ(nullptr, prefs_.GetUserPrefValue(kSyncRequested));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(nullptr, prefs_.GetUserPrefValue(
                         syncer::prefs::internal::kSyncDisabledViaDashboard));
#endif
}

TEST_F(BrowserPrefsTest, MigrateObsoleteProfilePrefSyncRequestedSetToTrue) {
  prefs_.SetBoolean(kSyncRequested, true);
  MigrateObsoleteProfilePrefs(&prefs_);
  EXPECT_EQ(nullptr, prefs_.GetUserPrefValue(kSyncRequested));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(nullptr, prefs_.GetUserPrefValue(
                         syncer::prefs::internal::kSyncDisabledViaDashboard));
#endif
}

TEST_F(BrowserPrefsTest, MigrateObsoleteProfilePrefSyncRequestedSetToFalse) {
  prefs_.SetBoolean(kSyncRequested, false);
  MigrateObsoleteProfilePrefs(&prefs_);
  EXPECT_EQ(nullptr, prefs_.GetUserPrefValue(kSyncRequested));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_NE(nullptr, prefs_.GetUserPrefValue(
                         syncer::prefs::internal::kSyncDisabledViaDashboard));
  EXPECT_TRUE(
      prefs_.GetBoolean(syncer::prefs::internal::kSyncDisabledViaDashboard));
#endif
}

}  // namespace
