// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/themes_helper.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "components/sync/driver/profile_sync_service.h"
#include "content/public/test/test_utils.h"

using themes_helper::GetCustomTheme;
using themes_helper::GetThemeID;
using themes_helper::UseDefaultTheme;
using themes_helper::UseSystemTheme;
using themes_helper::UsingCustomTheme;
using themes_helper::UsingDefaultTheme;
using themes_helper::UsingSystemTheme;

namespace {

class SingleClientThemesSyncTest : public SyncTest {
 public:
  SingleClientThemesSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientThemesSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientThemesSyncTest);
};

// TODO(akalin): Add tests for model association (i.e., tests that
// start with SetupClients(), change the theme state, then call
// SetupSync()).

IN_PROC_BROWSER_TEST_F(SingleClientThemesSyncTest, CustomTheme) {
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

IN_PROC_BROWSER_TEST_F(SingleClientThemesSyncTest, NativeTheme) {
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

IN_PROC_BROWSER_TEST_F(SingleClientThemesSyncTest, DefaultTheme) {
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

}  // namespace
