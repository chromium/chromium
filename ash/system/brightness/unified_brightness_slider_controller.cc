// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/unified_brightness_slider_controller.h"

#include "ash/shell.h"
#include "ash/system/brightness/unified_brightness_view.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/scoped_refptr.h"

namespace ash {
namespace {

// We don't let the screen brightness go lower than this when it's being
// adjusted via the slider.  Otherwise, if the user doesn't know about the
// brightness keys, they may turn the backlight off and not know how to turn it
// back on.
constexpr double kMinBrightnessPercent = 5.0;

}  // namespace

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
    return;
  }
  // We have to store previous manually set value because |old_value| might be
  // set by UnifiedSystemTrayModel::Observer.
  previous_percent_ = percent;

  percent = std::max(kMinBrightnessPercent, percent);
  brightness_control_delegate->SetBrightnessPercent(percent, true);
}

}  // namespace ash
