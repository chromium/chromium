// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_THEME_WATCHER_WIN_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_THEME_WATCHER_WIN_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/win/registry.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

// Tracks whether the Windows taskbar, which hosts the status tray, uses the
// dark theme, so that status icons can switch between light and dark images.
class StatusTrayThemeWatcherWin : public ui::NativeThemeObserver {
 public:
  // The registry key, under HKEY_CURRENT_USER, whose "SystemUsesLightTheme"
  // value says whether the taskbar uses the light theme.
  static constexpr wchar_t kThemesRegkeyPath[] =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

  // `on_theme_changed` runs whenever `in_dark_mode()` changes.
  explicit StatusTrayThemeWatcherWin(base::RepeatingClosure on_theme_changed);

  StatusTrayThemeWatcherWin(const StatusTrayThemeWatcherWin&) = delete;
  StatusTrayThemeWatcherWin& operator=(const StatusTrayThemeWatcherWin&) =
      delete;

  ~StatusTrayThemeWatcherWin() override;

  bool in_dark_mode() const { return in_dark_mode_; }

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

 private:
  void RegisterThemesRegkeyObserver();
  bool IsDarkModeInThemesRegkey() const;
  void SetInDarkMode(bool in_dark_mode);

  base::RepeatingClosure on_theme_changed_;

  // System light/dark mode registry key.
  base::win::RegKey hkcu_themes_regkey_;

  // Theme change observer. Used only if registry key cannot be opened.
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observer_{this};

  // Whether the taskbar is in dark mode. Read from the registry key if it can
  // be opened, and from the native theme otherwise.
  bool in_dark_mode_ = false;
};

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_THEME_WATCHER_WIN_H_
