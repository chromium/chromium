// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_MIC_GAIN_SLIDER_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_MIC_GAIN_SLIDER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/unified/unified_slider_view.h"
#include "base/callback_forward.h"

namespace ash {

class MicGainSliderView;

// Controller for mic gain sliders situated in audio detailed
// view in the system tray.
class ASH_EXPORT MicGainSliderController : public UnifiedSliderListener {
 public:
  MicGainSliderController();
  ~MicGainSliderController() override;

  // Create a slider view for a specific input device.
  std::unique_ptr<MicGainSliderView> CreateMicGainSlider(uint64_t device_id,
                                                         bool internal);

  using MapDeviceSliderCallback =
      base::RepeatingCallback<void(uint64_t, views::View*)>;
  static void SetMapDeviceSliderCallbackForTest(
      MapDeviceSliderCallback* test_slider_device_callback);

  // UnifiedSliderListener:
  views::View* CreateView() override;
  QsSliderCatalogName GetCatalogName() override;
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

  void SliderButtonPressed();
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_MIC_GAIN_SLIDER_CONTROLLER_H_
