// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/incognito_mode_prefs.h"

#include "base/test/gtest_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

class IncognitoModePrefsTest : public testing::Test {
 protected:
  void SetUp() override {
    IncognitoModePrefs::RegisterProfilePrefs(prefs_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(IncognitoModePrefsTest, IntToAvailability) {
  ASSERT_EQ(0, static_cast<int>(policy::IncognitoModeAvailability::kEnabled));
  ASSERT_EQ(1, static_cast<int>(policy::IncognitoModeAvailability::kDisabled));
  ASSERT_EQ(2, static_cast<int>(policy::IncognitoModeAvailability::kForced));

  policy::IncognitoModeAvailability incognito;
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(0, &incognito));
  EXPECT_EQ(policy::IncognitoModeAvailability::kEnabled, incognito);
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(1, &incognito));
  EXPECT_EQ(policy::IncognitoModeAvailability::kDisabled, incognito);
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(2, &incognito));
  EXPECT_EQ(policy::IncognitoModeAvailability::kForced, incognito);

  EXPECT_FALSE(IncognitoModePrefs::IntToAvailability(10, &incognito));
  EXPECT_EQ(IncognitoModePrefs::kDefaultAvailability, incognito);
  EXPECT_FALSE(IncognitoModePrefs::IntToAvailability(-1, &incognito));
  EXPECT_EQ(IncognitoModePrefs::kDefaultAvailability, incognito);
}

TEST_F(IncognitoModePrefsTest, GetAvailability) {
  prefs_.SetUserPref(policy::policy_prefs::kIncognitoModeAvailability,
                     std::make_unique<base::Value>(static_cast<int>(
                         policy::IncognitoModeAvailability::kEnabled)));
  EXPECT_EQ(policy::IncognitoModeAvailability::kEnabled,
            IncognitoModePrefs::GetAvailability(&prefs_));

  prefs_.SetUserPref(policy::policy_prefs::kIncognitoModeAvailability,
                     std::make_unique<base::Value>(static_cast<int>(
                         policy::IncognitoModeAvailability::kDisabled)));
  EXPECT_EQ(policy::IncognitoModeAvailability::kDisabled,
            IncognitoModePrefs::GetAvailability(&prefs_));

  prefs_.SetUserPref(policy::policy_prefs::kIncognitoModeAvailability,
                     std::make_unique<base::Value>(static_cast<int>(
                         policy::IncognitoModeAvailability::kForced)));
  EXPECT_EQ(policy::IncognitoModeAvailability::kForced,
            IncognitoModePrefs::GetAvailability(&prefs_));
}

typedef IncognitoModePrefsTest IncognitoModePrefsDeathTest;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_GetAvailabilityBadValue DISABLED_GetAvailabilityBadValue
#else
#define MAYBE_GetAvailabilityBadValue GetAvailabilityBadValue
#endif
TEST_F(IncognitoModePrefsDeathTest, MAYBE_GetAvailabilityBadValue) {
  prefs_.SetUserPref(policy::policy_prefs::kIncognitoModeAvailability,
                     std::make_unique<base::Value>(-1));
  EXPECT_DCHECK_DEATH({
    policy::IncognitoModeAvailability availability =
        IncognitoModePrefs::GetAvailability(&prefs_);
    EXPECT_EQ(policy::IncognitoModeAvailability::kEnabled, availability);
  });
}
