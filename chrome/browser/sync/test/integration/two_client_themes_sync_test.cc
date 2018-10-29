// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/themes_helper.h"
#include "components/sync/driver/sync_driver_switches.h"

namespace {

using themes_helper::GetCustomTheme;
using themes_helper::GetThemeID;
using themes_helper::UseCustomTheme;
using themes_helper::UseDefaultTheme;
using themes_helper::UseSystemTheme;
using themes_helper::UsingCustomTheme;
using themes_helper::UsingDefaultTheme;
using themes_helper::UsingSystemTheme;

class TwoClientThemesSyncTest : public FeatureToggler, public SyncTest {
 public:
  TwoClientThemesSyncTest()
      : FeatureToggler(switches::kSyncPseudoUSSThemes), SyncTest(TWO_CLIENT) {}
  ~TwoClientThemesSyncTest() override {}

  bool TestUsesSelfNotifications() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientThemesSyncTest);
};

// Starts with default themes, then sets up sync and uses it to set all
// profiles to use a custom theme.  Does not actually install any themes, but
// instead verifies the custom theme is pending for install.
IN_PROC_BROWSER_TEST_P(TwoClientThemesSyncTest,
                       E2E_ENABLED(DefaultThenSyncCustom)) {
  ASSERT_TRUE(SetupSync());

  ASSERT_FALSE(UsingCustomTheme(GetProfile(0)));
  ASSERT_FALSE(UsingCustomTheme(GetProfile(1)));

  SetCustomTheme(GetProfile(0));
  ASSERT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));

  // TODO(sync): Add functions to simulate when a pending extension
  // is installed as well as when a pending extension fails to
  // install.
  ASSERT_TRUE(
      ThemePendingInstallChecker(GetProfile(1), GetCustomTheme(0)).Wait());

  EXPECT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  EXPECT_FALSE(UsingCustomTheme(GetProfile(1)));
}

// Starts with custom themes, then sets up sync and uses it to set all profiles
// to the system theme.
IN_PROC_BROWSER_TEST_P(TwoClientThemesSyncTest,
                       E2E_ENABLED(CustomThenSyncNative)) {
  ASSERT_TRUE(SetupClients());

  SetCustomTheme(GetProfile(0));
  SetCustomTheme(GetProfile(1));

  ASSERT_TRUE(SetupSync());

  UseSystemTheme(GetProfile(0));
  ASSERT_TRUE(UsingSystemTheme(GetProfile(0)));

  ASSERT_TRUE(SystemThemeChecker(GetProfile(1)).Wait());

  EXPECT_TRUE(UsingSystemTheme(GetProfile(0)));
  EXPECT_TRUE(UsingSystemTheme(GetProfile(1)));
}

// Starts with custom themes, then sets up sync and uses it to set all profiles
// to the default theme.
IN_PROC_BROWSER_TEST_P(TwoClientThemesSyncTest,
                       E2E_ENABLED(CustomThenSyncDefault)) {
  ASSERT_TRUE(SetupClients());

  SetCustomTheme(GetProfile(0));
  SetCustomTheme(GetProfile(1));

  ASSERT_TRUE(SetupSync());

  UseDefaultTheme(GetProfile(0));
  EXPECT_TRUE(UsingDefaultTheme(GetProfile(0)));

  ASSERT_TRUE(DefaultThemeChecker(GetProfile(1)).Wait());
  EXPECT_TRUE(UsingDefaultTheme(GetProfile(0)));
  EXPECT_TRUE(UsingDefaultTheme(GetProfile(1)));
}

// Cycles through a set of options.
//
// Most other tests have significant coverage of model association.  This test
// is intended to test steady-state scenarios.
IN_PROC_BROWSER_TEST_P(TwoClientThemesSyncTest, E2E_ENABLED(CycleOptions)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetCustomTheme(GetProfile(0));

  ASSERT_TRUE(
      ThemePendingInstallChecker(GetProfile(1), GetCustomTheme(0)).Wait());
  EXPECT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));

  UseSystemTheme(GetProfile(0));

  ASSERT_TRUE(SystemThemeChecker(GetProfile(1)).Wait());
  EXPECT_TRUE(UsingSystemTheme(GetProfile(0)));
  EXPECT_TRUE(UsingSystemTheme(GetProfile(1)));

  UseDefaultTheme(GetProfile(0));

  ASSERT_TRUE(DefaultThemeChecker(GetProfile(1)).Wait());
  EXPECT_TRUE(UsingDefaultTheme(GetProfile(0)));
  EXPECT_TRUE(UsingDefaultTheme(GetProfile(1)));

  SetCustomTheme(GetProfile(0), 1);
  ASSERT_TRUE(
      ThemePendingInstallChecker(GetProfile(1), GetCustomTheme(1)).Wait());
  EXPECT_EQ(GetCustomTheme(1), GetThemeID(GetProfile(0)));
}

INSTANTIATE_TEST_CASE_P(USS,
                        TwoClientThemesSyncTest,
                        ::testing::Values(false, true));

}  // namespace
