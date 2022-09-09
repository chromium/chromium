// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/incognito_mode_prefs.h"

#include "base/test/gtest_util.h"
#include "chrome/common/pref_names.h"
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
  ASSERT_EQ(0, static_cast<int>(IncognitoModePrefs::Availability::kEnabled));
  ASSERT_EQ(1, static_cast<int>(IncognitoModePrefs::Availability::kDisabled));
  ASSERT_EQ(2, static_cast<int>(IncognitoModePrefs::Availability::kForced));

  IncognitoModePrefs::Availability incognito;
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(0, &incognito));
  EXPECT_EQ(IncognitoModePrefs::Availability::kEnabled, incognito);
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(1, &incognito));
  EXPECT_EQ(IncognitoModePrefs::Availability::kDisabled, incognito);
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(2, &incognito));
  EXPECT_EQ(IncognitoModePrefs::Availability::kForced, incognito);

  EXPECT_FALSE(IncognitoModePrefs::IntToAvailability(10, &incognito));
  EXPECT_EQ(IncognitoModePrefs::kDefaultAvailability, incognito);
  EXPECT_FALSE(IncognitoModePrefs::IntToAvailability(-1, &incognito));
  EXPECT_EQ(IncognitoModePrefs::kDefaultAvailability, incognito);
}

TEST_F(IncognitoModePrefsTest, GetAvailability) {
  prefs_.SetUserPref(prefs::kIncognitoModeAvailability,
                     std::make_unique<base::Value>(static_cast<int>(
                         IncognitoModePrefs::Availability::kEnabled)));
  EXPECT_EQ(IncognitoModePrefs::Availability::kEnabled,
            IncognitoModePrefs::GetAvailability(&prefs_));

  prefs_.SetUserPref(prefs::kIncognitoModeAvailability,
                     std::make_unique<base::Value>(static_cast<int>(
                         IncognitoModePrefs::Availability::kDisabled)));
  EXPECT_EQ(IncognitoModePrefs::Availability::kDisabled,
            IncognitoModePrefs::GetAvailability(&prefs_));

  prefs_.SetUserPref(prefs::kIncognitoModeAvailability,
                     std::make_unique<base::Value>(static_cast<int>(
                         IncognitoModePrefs::Availability::kForced)));
  EXPECT_EQ(IncognitoModePrefs::Availability::kForced,
            IncognitoModePrefs::GetAvailability(&prefs_));
}

typedef IncognitoModePrefsTest IncognitoModePrefsDeathTest;

TEST_F(IncognitoModePrefsDeathTest, GetAvailabilityBadValue) {
  prefs_.SetUserPref(prefs::kIncognitoModeAvailability,
                     std::make_unique<base::Value>(-1));
  EXPECT_DCHECK_DEATH({
    IncognitoModePrefs::Availability availability =
        IncognitoModePrefs::GetAvailability(&prefs_);
    EXPECT_EQ(IncognitoModePrefs::Availability::kEnabled, availability);
  });
}
