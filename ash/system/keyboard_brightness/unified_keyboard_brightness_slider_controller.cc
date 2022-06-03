// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/unified_keyboard_brightness_slider_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/unified_system_tray_model.h"

namespace ash {

namespace {

class UnifiedKeyboardBrightnessView : public UnifiedSliderView,
                                      public UnifiedSystemTrayModel::Observer {
 public:
  UnifiedKeyboardBrightnessView(
      UnifiedKeyboardBrightnessSliderController* controller,
      UnifiedSystemTrayModel* model)
      : UnifiedSliderView(views::Button::PressedCallback(),
                          controller,
                          kUnifiedMenuKeyboardBrightnessIcon,
                          IDS_ASH_STATUS_TRAY_BRIGHTNESS,
                          true /* readonly*/),
        model_(model) {
    model_->AddObserver(this);
    OnKeyboardBrightnessChanged(false /* by_user */);
  }

  UnifiedKeyboardBrightnessView(const UnifiedKeyboardBrightnessView&) = delete;
  UnifiedKeyboardBrightnessView& operator=(
      const UnifiedKeyboardBrightnessView&) = delete;

  ~UnifiedKeyboardBrightnessView() override { model_->RemoveObserver(this); }

  // UnifiedSystemTrayModel::Observer:
  void OnKeyboardBrightnessChanged(bool by_user) override {
    SetSliderValue(model_->keyboard_brightness(), by_user);
  }

 private:
  UnifiedSystemTrayModel* const model_;
};

}  // namespace

UnifiedKeyboardBrightnessSliderController::
    UnifiedKeyboardBrightnessSliderController(UnifiedSystemTrayModel* model)
    : model_(model) {}

UnifiedKeyboardBrightnessSliderController::
    ~UnifiedKeyboardBrightnessSliderController() = default;

views::View* UnifiedKeyboardBrightnessSliderController::CreateView() {
  DCHECK(!slider_);
  slider_ = new UnifiedKeyboardBrightnessView(this, model_);
  return slider_;
}

void UnifiedKeyboardBrightnessSliderController::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  // This slider is read-only.
}

}  // namespace ash
