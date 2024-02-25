// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_SLIDER_CONTROLLER_H_
#define ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_SLIDER_CONTROLLER_H_

#include <memory>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/unified_slider_view.h"
#include "base/dcheck_is_on.h"
#include "base/memory/scoped_refptr.h"

namespace ash {

class UnifiedSystemTrayModel;
class UnifiedBrightnessView;

// Controller of a slider that can change display brightness.
class ASH_EXPORT UnifiedBrightnessSliderController
    : public UnifiedSliderListener {
 public:
  UnifiedBrightnessSliderController(scoped_refptr<UnifiedSystemTrayModel> model,
                                    views::Button::PressedCallback callback);

  UnifiedBrightnessSliderController(const UnifiedBrightnessSliderController&) =
      delete;
  UnifiedBrightnessSliderController& operator=(
      const UnifiedBrightnessSliderController&) = delete;

  ~UnifiedBrightnessSliderController() override;

  // Creates a slider view for the brightness slider in `DisplayDetailedView`.
  std::unique_ptr<UnifiedBrightnessView> CreateBrightnessSlider();

  // UnifiedSliderListener:
  std::unique_ptr<UnifiedSliderView> CreateView() override;
  QsSliderCatalogName GetCatalogName() override;
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

 private:
  scoped_refptr<UnifiedSystemTrayModel> model_;
  views::Button::PressedCallback callback_;

  // We have to store previous manually set value because |old_value| might be
  // set by UnifiedSystemTrayModel::Observer.
  double previous_percent_ = 100.0;

#if DCHECK_IS_ON()
  bool created_view_ = false;
#endif
};

}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_SLIDER_CONTROLLER_H_
