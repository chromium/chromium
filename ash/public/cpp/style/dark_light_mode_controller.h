// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_STYLE_DARK_LIGHT_MODE_CONTROLLER_H_
#define ASH_PUBLIC_CPP_STYLE_DARK_LIGHT_MODE_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

class ColorModeObserver;

// An interface implemented by Ash that controls the behavior of dark/light
// mode. See DarkLightModeControllerImpl for more details.
class ASH_PUBLIC_EXPORT DarkLightModeController {
 public:
  static DarkLightModeController* Get();

  virtual void AddObserver(ColorModeObserver* observer) = 0;
  virtual void RemoveObserver(ColorModeObserver* observer) = 0;

  // True if the current color mode is DARK. By default, color mode is AUTO when
  // #DarkLightMode feature is enabled, which means LIGHT after sunrise and
  // before sunset, DARK otherwise. And the default color mode will be DARK
  // when the #DarkLightMode feature is disabled, as most of the SysUI surfaces
  // are dark without dark/light enabled. But it can be overridden by
  // ScopedLightModeAsDefault if the surface wants to keep as LIGHT in the case.
  // See `override_light_mode_as_default_` for more details.
  virtual bool IsDarkModeEnabled() const = 0;

  // Enables or disables dark mode for testing. Only works when the
  // DarkLightMode feature is enabled.
  virtual void SetDarkModeEnabledForTest(bool enabled) = 0;

 protected:
  DarkLightModeController();
  virtual ~DarkLightModeController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_STYLE_DARK_LIGHT_MODE_CONTROLLER_H_
