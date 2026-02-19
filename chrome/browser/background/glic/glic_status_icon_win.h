// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_WIN_H_
#define CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_WIN_H_

#include "base/scoped_observation.h"
#include "base/win/registry.h"
#include "chrome/browser/background/glic/glic_status_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

class StatusTray;

namespace glic {

class GlicController;

class GlicStatusIconWin : public GlicStatusIcon,
                          public ui::NativeThemeObserver {
 public:
  GlicStatusIconWin(GlicController* controller, StatusTray* status_tray);

  GlicStatusIconWin(const GlicStatusIconWin&) = delete;
  GlicStatusIconWin& operator=(const GlicStatusIconWin&) = delete;

  ~GlicStatusIconWin() override;

  // GlicStatusIcon:
  void Init() override;

  // ui::NativeThemeObserver
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

 private:
  // GlicStatusIcon:
  gfx::ImageSkia GetIcon() const override;

  void RegisterThemesRegkeyObserver();
  void UpdateForThemesRegkey();

  // System light/dark mode registry key.
  base::win::RegKey hkcu_themes_regkey_;

  // Theme change observer. Used only if registry key cannot be opened.
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observer_{this};

  // Whether the system is in dark mode. The registry key takes precedence, if
  // available.
  bool in_dark_mode_ =
      ui::NativeTheme::GetInstanceForNativeUi()->preferred_color_scheme() ==
      ui::NativeTheme::PreferredColorScheme::kDark;
};

}  // namespace glic

#endif  // CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_WIN_H_
