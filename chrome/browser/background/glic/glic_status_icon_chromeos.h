// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_CHROMEOS_H_
#define CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_CHROMEOS_H_

#include "base/scoped_observation.h"
#include "chrome/browser/background/glic/glic_status_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

class StatusTray;

namespace glic {

class GlicController;

class GlicStatusIconChromeOS : public GlicStatusIcon,
                               public ui::NativeThemeObserver {
 public:
  GlicStatusIconChromeOS(GlicController* controller, StatusTray* status_tray);

  GlicStatusIconChromeOS(const GlicStatusIconChromeOS&) = delete;
  GlicStatusIconChromeOS& operator=(const GlicStatusIconChromeOS&) = delete;

  ~GlicStatusIconChromeOS() override;

  // ui::NativeThemeObserver
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

 private:
  // GlicStatusIcon:
  gfx::ImageSkia GetIcon() const override;

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observer_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_CHROMEOS_H_
