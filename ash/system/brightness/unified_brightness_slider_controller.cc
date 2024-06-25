// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/unified_brightness_slider_controller.h"

#include <memory>
#include <utility>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/shell.h"
#include "ash/system/brightness/unified_brightness_view.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/dcheck_is_on.h"
#include "base/memory/scoped_refptr.h"

namespace ash {

namespace {

// We don't let the screen brightness go lower than this when it's being
// adjusted via the slider.  Otherwise, if the user doesn't know about the
// brightness keys, they may turn the backlight off and not know how to turn
// it back on.
static constexpr double kMinBrightnessPercent = 5.0;

}  // namespace

UnifiedBrightnessSliderController::UnifiedBrightnessSliderController(
    scoped_refptr<UnifiedSystemTrayModel> model,
    views::Button::PressedCallback callback)
    : model_(model), callback_(std::move(callback)) {}

UnifiedBrightnessSliderController::~UnifiedBrightnessSliderController() =
    default;

std::unique_ptr<UnifiedBrightnessView>
UnifiedBrightnessSliderController::CreateBrightnessSlider() {
  return std::make_unique<UnifiedBrightnessView>(this, model_);
}

std::unique_ptr<UnifiedSliderView>
UnifiedBrightnessSliderController::CreateView() {
#if DCHECK_IS_ON()
  DCHECK(!created_view_);
  created_view_ = true;
#endif
  // Consuming `callback_` is safe here; `CreateView()` should only be called
  // once per controller instance per the DCHECK() above.
  return std::make_unique<UnifiedBrightnessView>(this, model_,
                                                 std::move(callback_));
}

QsSliderCatalogName UnifiedBrightnessSliderController::GetCatalogName() {
  return QsSliderCatalogName::kBrightness;
}

void UnifiedBrightnessSliderController::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  if (reason != views::SliderChangeReason::kByUser) {
    return;
  }

  BrightnessControlDelegate* brightness_control_delegate =
      Shell::Get()->brightness_control_delegate();
  if (!brightness_control_delegate) {
    return;
  }

  double percent = value * 100.;
  // If previous percentage and current percentage are both below the minimum,
  // we don't update the actual brightness.
  if (percent < kMinBrightnessPercent &&
      previous_percent_ < kMinBrightnessPercent) {
    return;
  }

  if (previous_percent_ != percent) {
    TrackValueChangeUMA(/*going_up=*/percent > previous_percent_);
  }

  // We have to store previous manually set value because |old_value| might be
  // set by UnifiedSystemTrayModel::Observer.
  previous_percent_ = percent;

  percent = std::max(kMinBrightnessPercent, percent);
  brightness_control_delegate->SetBrightnessPercent(
      percent, /*gradual=*/true, /*source=*/
      BrightnessControlDelegate::BrightnessChangeSource::kQuickSettings);
}

}  // namespace ash
