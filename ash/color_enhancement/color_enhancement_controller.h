// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COLOR_ENHANCEMENT_COLOR_ENHANCEMENT_CONTROLLER_H_
#define ASH_COLOR_ENHANCEMENT_COLOR_ENHANCEMENT_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"

namespace aura {
class Window;
}

namespace ash {

// Controls the color enhancement options on all displays. These options
// are applied globally.
class ASH_EXPORT ColorEnhancementController : public ShellObserver {
 public:
  ColorEnhancementController();

  ColorEnhancementController(const ColorEnhancementController&) = delete;
  ColorEnhancementController& operator=(const ColorEnhancementController&) =
      delete;

  ~ColorEnhancementController() override;

  // Sets high contrast mode and updates all available displays.
  void SetHighContrastEnabled(bool enabled);

  // Sets greyscale amount and updates all available displays.
  void SetGreyscaleAmount(float amount);

  // Sets saturation amount and updates all available displays.
  void SetSaturationAmount(float amount);

  // Sets sepia amount and updates all available displays.
  void SetSepiaAmount(float amount);

  // Sets hue rotation amount and updates all available displays.
  void SetHueRotationAmount(int amount);

  bool ShouldEnableCursorCompositingForSepia() const;

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;

 private:
  // Updates all active displays.
  void UpdateAllDisplays();

  // Updates high contrast mode on the display associated with the passed
  // `root_window`.
  void UpdateDisplay(aura::Window* root_window);

  // Indicates if the high contrast mode is enabled or disabled.
  bool high_contrast_enabled_ = false;

  // Amount of hue rotation, on the scale of 0 to 359.
  int hue_rotation_amount_ = 0;

  // Amount of greyscale, on the scale of 0 to 1.
  float greyscale_amount_ = 0;

  // Amount of sepia, on the scale of 0 to 1.
  float sepia_amount_ = 0;

  // Amount of saturation where 1 is normal. Values may range from
  // 0 to max float.
  float saturation_amount_ = 1;
};

}  // namespace ash

#endif  // ASH_COLOR_ENHANCEMENT_COLOR_ENHANCEMENT_CONTROLLER_H_
