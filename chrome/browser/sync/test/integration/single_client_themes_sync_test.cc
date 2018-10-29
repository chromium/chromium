// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/themes_helper.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "content/public/test/test_utils.h"

using themes_helper::GetCustomTheme;
using themes_helper::GetThemeID;
using themes_helper::UseDefaultTheme;
using themes_helper::UseSystemTheme;
using themes_helper::UsingCustomTheme;
using themes_helper::UsingDefaultTheme;
using themes_helper::UsingSystemTheme;

namespace {

class SingleClientThemesSyncTest : public FeatureToggler, public SyncTest {
 public:
  SingleClientThemesSyncTest()
      : FeatureToggler(switches::kSyncPseudoUSSThemes),
        SyncTest(SINGLE_CLIENT) {}
  ~SingleClientThemesSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientThemesSyncTest);
};

// TODO(akalin): Add tests for model association (i.e., tests that
// start with SetupClients(), change the theme state, then call
// SetupSync()).

IN_PROC_BROWSER_TEST_P(SingleClientThemesSyncTest, CustomTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  EXPECT_FALSE(UsingCustomTheme(GetProfile(0)));
  EXPECT_FALSE(UsingCustomTheme(verifier()));

  SetCustomTheme(GetProfile(0));
  SetCustomTheme(verifier());
  EXPECT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  EXPECT_EQ(GetCustomTheme(0), GetThemeID(verifier()));

  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  EXPECT_EQ(GetCustomTheme(0), GetThemeID(GetProfile(0)));
  EXPECT_EQ(GetCustomTheme(0), GetThemeID(verifier()));
}

// TODO(sync): Fails on Chrome OS. See http://crbug.com/84575.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(SingleClientThemesSyncTest, DISABLED_NativeTheme) {
#else
IN_PROC_BROWSER_TEST_P(SingleClientThemesSyncTest, NativeTheme) {
#endif  // OS_CHROMEOS
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetCustomTheme(GetProfile(0));
  SetCustomTheme(verifier());
  EXPECT_FALSE(UsingSystemTheme(GetProfile(0)));
  EXPECT_FALSE(UsingSystemTheme(verifier()));

  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  UseSystemTheme(GetProfile(0));
  UseSystemTheme(verifier());
  EXPECT_TRUE(UsingSystemTheme(GetProfile(0)));
  EXPECT_TRUE(UsingSystemTheme(verifier()));

  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  EXPECT_TRUE(UsingSystemTheme(GetProfile(0)));
  EXPECT_TRUE(UsingSystemTheme(verifier()));
}

IN_PROC_BROWSER_TEST_P(SingleClientThemesSyncTest, DefaultTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetCustomTheme(GetProfile(0));
  EXPECT_FALSE(UsingDefaultTheme(GetProfile(0)));

  SetCustomTheme(verifier());
  EXPECT_FALSE(UsingDefaultTheme(verifier()));

  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  UseDefaultTheme(GetProfile(0));
  EXPECT_TRUE(UsingDefaultTheme(GetProfile(0)));
  UseDefaultTheme(verifier());
  EXPECT_TRUE(UsingDefaultTheme(verifier()));

  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  EXPECT_TRUE(UsingDefaultTheme(GetProfile(0)));
  EXPECT_TRUE(UsingDefaultTheme(verifier()));
}

INSTANTIATE_TEST_CASE_P(USS,
                        SingleClientThemesSyncTest,
                        ::testing::Values(false, true));

}  // namespace
