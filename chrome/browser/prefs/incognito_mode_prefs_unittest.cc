// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  ASSERT_EQ(0, IncognitoModePrefs::ENABLED);
  ASSERT_EQ(1, IncognitoModePrefs::DISABLED);
  ASSERT_EQ(2, IncognitoModePrefs::FORCED);

  IncognitoModePrefs::Availability incognito;
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(0, &incognito));
  EXPECT_EQ(IncognitoModePrefs::ENABLED, incognito);
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(1, &incognito));
  EXPECT_EQ(IncognitoModePrefs::DISABLED, incognito);
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(2, &incognito));
  EXPECT_EQ(IncognitoModePrefs::FORCED, incognito);

  EXPECT_FALSE(IncognitoModePrefs::IntToAvailability(10, &incognito));
  EXPECT_EQ(IncognitoModePrefs::kDefaultAvailability, incognito);
  EXPECT_FALSE(IncognitoModePrefs::IntToAvailability(-1, &incognito));
  EXPECT_EQ(IncognitoModePrefs::kDefaultAvailability, incognito);
}

TEST_F(IncognitoModePrefsTest, GetAvailability) {
  prefs_.SetUserPref(
      prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(IncognitoModePrefs::ENABLED));
  EXPECT_EQ(IncognitoModePrefs::ENABLED,
            IncognitoModePrefs::GetAvailability(&prefs_));

  prefs_.SetUserPref(
      prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(IncognitoModePrefs::DISABLED));
  EXPECT_EQ(IncognitoModePrefs::DISABLED,
            IncognitoModePrefs::GetAvailability(&prefs_));

  prefs_.SetUserPref(prefs::kIncognitoModeAvailability,
                     std::make_unique<base::Value>(IncognitoModePrefs::FORCED));
  EXPECT_EQ(IncognitoModePrefs::FORCED,
            IncognitoModePrefs::GetAvailability(&prefs_));
}

typedef IncognitoModePrefsTest IncognitoModePrefsDeathTest;

TEST_F(IncognitoModePrefsDeathTest, GetAvailabilityBadValue) {
  prefs_.SetUserPref(prefs::kIncognitoModeAvailability,
                     std::make_unique<base::Value>(-1));
  EXPECT_DCHECK_DEATH({
    IncognitoModePrefs::Availability availability =
        IncognitoModePrefs::GetAvailability(&prefs_);
    EXPECT_EQ(IncognitoModePrefs::ENABLED, availability);
  });
}
