// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_KEYBOARD_BRIGHTNESS_UNIFIED_KEYBOARD_BRIGHTNESS_SLIDER_CONTROLLER_H_
#define ASH_SYSTEM_KEYBOARD_BRIGHTNESS_UNIFIED_KEYBOARD_BRIGHTNESS_SLIDER_CONTROLLER_H_

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/unified_slider_view.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class UnifiedSystemTrayModel;

// Controller of a slider showing keyboard brightness.
class ASH_EXPORT UnifiedKeyboardBrightnessSliderController
    : public UnifiedSliderListener {
 public:
  explicit UnifiedKeyboardBrightnessSliderController(
      UnifiedSystemTrayModel* model);

  UnifiedKeyboardBrightnessSliderController(
      const UnifiedKeyboardBrightnessSliderController&) = delete;
  UnifiedKeyboardBrightnessSliderController& operator=(
      const UnifiedKeyboardBrightnessSliderController&) = delete;

  ~UnifiedKeyboardBrightnessSliderController() override;

  // UnifiedSliderListener:
  std::unique_ptr<UnifiedSliderView> CreateView() override;
  QsSliderCatalogName GetCatalogName() override;
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

 private:
  const raw_ptr<UnifiedSystemTrayModel> model_;

#if DCHECK_IS_ON()
  bool created_view_ = false;
#endif
};

}  // namespace ash

#endif  // ASH_SYSTEM_KEYBOARD_BRIGHTNESS_UNIFIED_KEYBOARD_BRIGHTNESS_SLIDER_CONTROLLER_H_
