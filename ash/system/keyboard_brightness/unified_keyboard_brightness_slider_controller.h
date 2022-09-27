// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_KEYBOARD_BRIGHTNESS_UNIFIED_KEYBOARD_BRIGHTNESS_SLIDER_CONTROLLER_H_
#define ASH_SYSTEM_KEYBOARD_BRIGHTNESS_UNIFIED_KEYBOARD_BRIGHTNESS_SLIDER_CONTROLLER_H_

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/unified_slider_view.h"

namespace ash {

class UnifiedSystemTrayModel;

// Controller of a read-only slider showing keyboard brightness.
class UnifiedKeyboardBrightnessSliderController : public UnifiedSliderListener {
 public:
  explicit UnifiedKeyboardBrightnessSliderController(
      UnifiedSystemTrayModel* model);

  UnifiedKeyboardBrightnessSliderController(
      const UnifiedKeyboardBrightnessSliderController&) = delete;
  UnifiedKeyboardBrightnessSliderController& operator=(
      const UnifiedKeyboardBrightnessSliderController&) = delete;

  ~UnifiedKeyboardBrightnessSliderController() override;

  // UnifiedSliderListener:
  views::View* CreateView() override;
  QsSliderCatalogName GetCatalogName() override;
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

 private:
  UnifiedSystemTrayModel* const model_;
  UnifiedSliderView* slider_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_KEYBOARD_BRIGHTNESS_UNIFIED_KEYBOARD_BRIGHTNESS_SLIDER_CONTROLLER_H_
