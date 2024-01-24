// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_prefs.h"

#include <cstddef>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/sync/base/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kSyncRequested[] = "sync.requested";

#if !BUILDFLAG(IS_ANDROID)
constexpr char kExampleDomain[] = "example.com";
#endif

class BrowserPrefsTest : public testing::Test {
 protected:
  BrowserPrefsTest() { RegisterUserProfilePrefs(prefs_.registry()); }

  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(BrowserPrefsTest, MigrateObsoleteProfilePrefSyncRequestedDefaultValue) {
  MigrateObsoleteProfilePrefs(&prefs_, /*profile_path=*/base::FilePath());
  EXPECT_EQ(nullptr, prefs_.GetUserPrefValue(kSyncRequested));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(nullptr, prefs_.GetUserPrefValue(
                         syncer::prefs::internal::kSyncDisabledViaDashboard));
#endif
}

TEST_F(BrowserPrefsTest, MigrateObsoleteProfilePrefSyncRequestedSetToTrue) {
  prefs_.SetBoolean(kSyncRequested, true);
  MigrateObsoleteProfilePrefs(&prefs_, /*profile_path=*/base::FilePath());
  EXPECT_EQ(nullptr, prefs_.GetUserPrefValue(kSyncRequested));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(nullptr, prefs_.GetUserPrefValue(
                         syncer::prefs::internal::kSyncDisabledViaDashboard));
#endif
}

TEST_F(BrowserPrefsTest, MigrateObsoleteProfilePrefSyncRequestedSetToFalse) {
  prefs_.SetBoolean(kSyncRequested, false);
  MigrateObsoleteProfilePrefs(&prefs_, /*profile_path=*/base::FilePath());
  EXPECT_EQ(nullptr, prefs_.GetUserPrefValue(kSyncRequested));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_NE(nullptr, prefs_.GetUserPrefValue(
                         syncer::prefs::internal::kSyncDisabledViaDashboard));
  EXPECT_TRUE(
      prefs_.GetBoolean(syncer::prefs::internal::kSyncDisabledViaDashboard));
#endif
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(BrowserPrefsTest, MigrateObsoleteProfilePrefTabDiscardingExceptions) {
  base::Value::List exclusion_list;
  exclusion_list.Append(kExampleDomain);
  prefs_.SetList(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptions,
      std::move(exclusion_list));
  MigrateObsoleteProfilePrefs(&prefs_, /*profile_path=*/base::FilePath());
  EXPECT_TRUE(
      prefs_
          .GetList(
              performance_manager::user_tuning::prefs::kTabDiscardingExceptions)
          .empty());

  base::Value::Dict discard_exceptions_map =
      prefs_
          .GetDict(performance_manager::user_tuning::prefs::
                       kTabDiscardingExceptionsWithTime)
          .Clone();
  EXPECT_TRUE(discard_exceptions_map.contains(kExampleDomain));
}
#endif

}  // namespace
