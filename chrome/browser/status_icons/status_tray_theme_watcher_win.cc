// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_tray_theme_watcher_win.h"

#include <windows.h>

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace {

bool IsDarkModeInNativeTheme(const ui::NativeTheme& native_theme) {
  return native_theme.preferred_color_scheme() ==
         ui::NativeTheme::PreferredColorScheme::kDark;
}

}  // namespace

StatusTrayThemeWatcherWin::StatusTrayThemeWatcherWin(
    base::RepeatingClosure on_theme_changed)
    : on_theme_changed_(std::move(on_theme_changed)) {
  if (hkcu_themes_regkey_.Open(HKEY_CURRENT_USER, kThemesRegkeyPath,
                               KEY_READ | KEY_NOTIFY) == ERROR_SUCCESS) {
    in_dark_mode_ = IsDarkModeInThemesRegkey();
    // If there's no sequenced task runner handle, we can't be called back for
    // registry changes. This generally happens in tests.
    if (base::SequencedTaskRunner::HasCurrentDefault()) {
      RegisterThemesRegkeyObserver();
    }
  } else {
    // Fall back to the native theme's preferred color scheme. Only query the
    // native theme here: creating it also creates the OS settings provider,
    // which watches its own HKCU registry keys.
    ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
    in_dark_mode_ = IsDarkModeInNativeTheme(*native_theme);
    native_theme_observer_.Observe(native_theme);
  }
}

StatusTrayThemeWatcherWin::~StatusTrayThemeWatcherWin() = default;

void StatusTrayThemeWatcherWin::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  CHECK(!hkcu_themes_regkey_.Valid());
  SetInDarkMode(IsDarkModeInNativeTheme(*observed_theme));
}

void StatusTrayThemeWatcherWin::RegisterThemesRegkeyObserver() {
  CHECK(hkcu_themes_regkey_.Valid());
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  hkcu_themes_regkey_.StartWatching(base::BindOnce(
      [](StatusTrayThemeWatcherWin* watcher) {
        // `StartWatching()`'s callback is one-shot and must be re-registered
        // for future notifications. Re-register before reading the key so a
        // change made in between is not missed, and before notifying, since
        // `on_theme_changed_` is outside code.
        watcher->RegisterThemesRegkeyObserver();
        watcher->SetInDarkMode(watcher->IsDarkModeInThemesRegkey());
      },
      base::Unretained(this)));
}

bool StatusTrayThemeWatcherWin::IsDarkModeInThemesRegkey() const {
  CHECK(hkcu_themes_regkey_.Valid());
  DWORD system_uses_light_theme = 1;
  hkcu_themes_regkey_.ReadValueDW(L"SystemUsesLightTheme",
                                  &system_uses_light_theme);
  return !system_uses_light_theme;
}

void StatusTrayThemeWatcherWin::SetInDarkMode(bool in_dark_mode) {
  if (in_dark_mode_ == in_dark_mode) {
    return;
  }
  in_dark_mode_ = in_dark_mode;
  on_theme_changed_.Run();
}
