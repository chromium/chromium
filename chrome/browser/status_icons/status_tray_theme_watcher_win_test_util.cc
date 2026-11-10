// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_tray_theme_watcher_win_test_util.h"

#include <windows.h>

#include "base/win/registry.h"
#include "chrome/browser/status_icons/status_tray_theme_watcher_win.h"
#include "testing/gtest/include/gtest/gtest.h"

void SetTaskbarDarkModeInRegistry(bool dark) {
  base::win::RegKey key(HKEY_CURRENT_USER,
                        StatusTrayThemeWatcherWin::kThemesRegkeyPath,
                        KEY_SET_VALUE);
  EXPECT_EQ(key.WriteValue(L"SystemUsesLightTheme", dark ? 0 : 1),
            ERROR_SUCCESS);
}
