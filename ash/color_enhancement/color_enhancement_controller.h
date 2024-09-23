// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COLOR_ENHANCEMENT_COLOR_ENHANCEMENT_CONTROLLER_H_
#define ASH_COLOR_ENHANCEMENT_COLOR_ENHANCEMENT_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "cc/paint/filter_operation.h"

namespace aura {
class Window;
}

namespace ash {

// The types of color filters available in color correction settings.
// Do not change these values, they are persisted to logs in
// ColorCorrectionFilterTypes enum and mapped in os settings.
enum ColorVisionCorrectionType {
  kProtanomaly = 0,
  kDeuteranomaly = 1,
  kTritanomaly = 2,
  kGrayscale = 3,
  kMaxValue = kGrayscale,
};

// Controls the color enhancement options on all displays. These options
// are applied globally.
class ASH_EXPORT ColorEnhancementController : public ShellObserver {
 public:
  ColorEnhancementController();

  ColorEnhancementController(const ColorEnhancementController&) = delete;
  ColorEnhancementController& operator=(const ColorEnhancementController&) =
      delete;

  ~ColorEnhancementController() override;

  // Sets high contrast mode (which inverts colors) and updates all available
  // displays.
  void SetHighContrastEnabled(bool enabled);

  // Sets whether color filtering options are enabled and updates all available
  // displays.
  void SetColorCorrectionEnabledAndUpdateDisplays(bool enabled);

  // Sets greyscale amount.
  void SetGreyscaleAmount(float amount);

  // Sets the color vision correction filter type and severity.
  // `severity` should be between 0 and 1.0, inclusive.
  void SetColorVisionCorrectionFilter(ColorVisionCorrectionType type,
                                      float severity);

  // Flashes the display when a notification occurs..
  void FlashScreenForNotification(bool show_flash, const SkColor& color);

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

  // Indicates if the color filtering options are enabled or disabled.
  bool color_filtering_enabled_ = false;

  // Amount of greyscale, on the scale of 0 to 1.
  float greyscale_amount_ = 0;

  // Color correction matrix.
  std::unique_ptr<cc::FilterOperation::Matrix> cvd_correction_matrix_;

  // Flash screen color.
  std::unique_ptr<cc::FilterOperation::Matrix> notification_flash_matrix_;
};

}  // namespace ash

#endif  // ASH_COLOR_ENHANCEMENT_COLOR_ENHANCEMENT_CONTROLLER_H_
