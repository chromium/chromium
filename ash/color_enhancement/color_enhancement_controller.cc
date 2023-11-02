// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/color_enhancement/color_enhancement_controller.h"

#include "ash/shell.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/compositor/layer.h"

namespace ash {

namespace {

// Sepia filter above .3 should enable cursor compositing. Beyond this point,
// users can perceive the mouse is too white if compositing does not occur.
// TODO (crbug.com/1031959): Check this value with UX to see if it can be
// larger.
const float kMinSepiaPerceptableDifference = 0.3f;

}  // namespace

ColorEnhancementController::ColorEnhancementController() {
  Shell::Get()->AddShellObserver(this);
}

ColorEnhancementController::~ColorEnhancementController() {
  Shell::Get()->RemoveShellObserver(this);
}

void ColorEnhancementController::SetHighContrastEnabled(bool enabled) {
  if (high_contrast_enabled_ == enabled)
    return;

  high_contrast_enabled_ = enabled;
  // Enable cursor compositing so the cursor is also inverted.
  Shell::Get()->UpdateCursorCompositingEnabled();
  UpdateAllDisplays();
}

void ColorEnhancementController::SetGreyscaleAmount(float amount) {
  if (greyscale_amount_ == amount || amount < 0 || amount > 1)
    return;

  greyscale_amount_ = amount;
  // Note: No need to do cursor compositing since cursors are greyscale already.
  UpdateAllDisplays();
}

void ColorEnhancementController::SetSaturationAmount(float amount) {
  if (saturation_amount_ == amount || amount < 0)
    return;

  saturation_amount_ = amount;
  // Note: No need to do cursor compositing since cursors are greyscale and not
  // impacted by saturation.
  UpdateAllDisplays();
}

void ColorEnhancementController::SetSepiaAmount(float amount) {
  if (sepia_amount_ == amount || amount < 0 || amount > 1)
    return;

  sepia_amount_ = amount;
  // The cursor should be tinted sepia as well. Update cursor compositing.
  Shell::Get()->UpdateCursorCompositingEnabled();
  UpdateAllDisplays();
}

void ColorEnhancementController::SetHueRotationAmount(int amount) {
  if (hue_rotation_amount_ == amount || amount < 0 || amount > 359)
    return;

  hue_rotation_amount_ = amount;
  // Note: No need to do cursor compositing since cursors are greyscale and not
  // impacted by hue rotation.
  UpdateAllDisplays();
}

bool ColorEnhancementController::ShouldEnableCursorCompositingForSepia() const {
  if (!::features::
          AreExperimentalAccessibilityColorEnhancementSettingsEnabled()) {
    return false;
  }

  // Enable cursor compositing if the sepia filter is on enough that
  // the white mouse cursor stands out. Sepia will not be set on the root
  // window if the setting value is greater than 1, so ignore that state.
  return sepia_amount_ >= kMinSepiaPerceptableDifference && sepia_amount_ <= 1;
}

void ColorEnhancementController::OnRootWindowAdded(aura::Window* root_window) {
  UpdateDisplay(root_window);
}

void ColorEnhancementController::UpdateAllDisplays() {
  for (auto* root_window : Shell::GetAllRootWindows())
    UpdateDisplay(root_window);
}

void ColorEnhancementController::UpdateDisplay(aura::Window* root_window) {
  ui::Layer* layer = root_window->layer();
  layer->SetLayerInverted(high_contrast_enabled_);

  if (!::features::
          AreExperimentalAccessibilityColorEnhancementSettingsEnabled()) {
    return;
  }

  layer->SetLayerGrayscale(greyscale_amount_);
  layer->SetLayerSaturation(saturation_amount_);
  layer->SetLayerSepia(sepia_amount_);
  layer->SetLayerHueRotation(hue_rotation_amount_);
  // TODO(crbug.com/1031959): Use SetLayerCustomColorMatrix to create color
  // filters for common color blindness types.
}

}  // namespace ash
