// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/unified_brightness_slider_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/shell.h"
#include "ash/system/brightness/unified_brightness_view.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/scoped_refptr.h"

namespace ash {

UnifiedBrightnessSliderController::UnifiedBrightnessSliderController(
    scoped_refptr<UnifiedSystemTrayModel> model)
    : model_(model) {}

UnifiedBrightnessSliderController::~UnifiedBrightnessSliderController() =
    default;

views::View* UnifiedBrightnessSliderController::CreateView() {
  DCHECK(!slider_);
  slider_ = new UnifiedBrightnessView(this, model_);
  return slider_;
}

QsSliderCatalogName UnifiedBrightnessSliderController::GetCatalogName() {
  return QsSliderCatalogName::kBrightness;
}

void UnifiedBrightnessSliderController::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  if (reason != views::SliderChangeReason::kByUser)
    return;

  BrightnessControlDelegate* brightness_control_delegate =
      Shell::Get()->brightness_control_delegate();
  if (!brightness_control_delegate)
    return;

  double percent = value * 100.;
  // If previous percentage and current percentage are both below the minimum,
  // we don't update the actual brightness.
  if (percent < kMinBrightnessPercent &&
      previous_percent_ < kMinBrightnessPercent) {
    // We still need to call `OnDisplayBrightnessChanged()` to update the icon
    // of the slider, we just don't update the brightness value.
    brightness_control_delegate->SetBrightnessPercent(previous_percent_, true);
    return;
  }

  if (previous_percent_ != percent) {
    TrackValueChangeUMA(/*going_up=*/percent > previous_percent_);
  }

  // We have to store previous manually set value because |old_value| might be
  // set by UnifiedSystemTrayModel::Observer.
  previous_percent_ = percent;

  percent = std::max(kMinBrightnessPercent, percent);
  brightness_control_delegate->SetBrightnessPercent(percent, true);
}

}  // namespace ash
