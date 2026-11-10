// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_THEME_WATCHER_WIN_TEST_UTIL_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_THEME_WATCHER_WIN_TEST_UTIL_H_

// Writes whether the taskbar uses the dark theme to the registry value that
// `StatusTrayThemeWatcherWin` watches, creating the key if needed. Override
// HKEY_CURRENT_USER first, e.g. with `registry_util::RegistryOverrideManager`.
void SetTaskbarDarkModeInRegistry(bool dark);

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_THEME_WATCHER_WIN_TEST_UTIL_H_
