// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_tray_theme_watcher_win.h"

#include <windows.h>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/browser/status_icons/status_tray_theme_watcher_win_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/native_theme/native_theme.h"

namespace {

class StatusTrayThemeWatcherWinTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  base::test::TaskEnvironment task_environment_;
  registry_util::RegistryOverrideManager registry_override_;
};

TEST_F(StatusTrayThemeWatcherWinTest, FollowsTaskbarTheme) {
  SetTaskbarDarkModeInRegistry(true);
  EXPECT_TRUE(StatusTrayThemeWatcherWin(base::DoNothing()).in_dark_mode());

  SetTaskbarDarkModeInRegistry(false);
  int change_count = 0;
  StatusTrayThemeWatcherWin watcher(
      base::BindLambdaForTesting([&] { ++change_count; }));
  ASSERT_FALSE(watcher.in_dark_mode());

  SetTaskbarDarkModeInRegistry(true);
  ASSERT_TRUE(base::test::RunUntil([&] { return watcher.in_dark_mode(); }));
  EXPECT_EQ(change_count, 1);

  SetTaskbarDarkModeInRegistry(false);
  ASSERT_TRUE(base::test::RunUntil([&] { return !watcher.in_dark_mode(); }));
  EXPECT_EQ(change_count, 2);
}

TEST_F(StatusTrayThemeWatcherWinTest, FallsBackToNativeThemeWithoutRegkey) {
  // The overridden registry has no themes key, so the watcher follows the
  // native theme instead.
  ui::MockOsSettingsProvider os_settings_provider;
  os_settings_provider.SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kLight);
  int change_count = 0;
  StatusTrayThemeWatcherWin watcher(
      base::BindLambdaForTesting([&] { ++change_count; }));
  ASSERT_FALSE(watcher.in_dark_mode());

  os_settings_provider.SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kDark);
  ASSERT_TRUE(base::test::RunUntil([&] { return watcher.in_dark_mode(); }));
  EXPECT_EQ(change_count, 1);

  os_settings_provider.SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kLight);
  ASSERT_TRUE(base::test::RunUntil([&] { return !watcher.in_dark_mode(); }));
  EXPECT_EQ(change_count, 2);
}

}  // namespace
