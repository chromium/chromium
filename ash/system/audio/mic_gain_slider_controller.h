// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_MIC_GAIN_SLIDER_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_MIC_GAIN_SLIDER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/unified/unified_slider_view.h"
#include "base/functional/callback_forward.h"
#include "base/timer/timer.h"

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
  std::unique_ptr<UnifiedSliderView> CreateView() override;
  QsSliderCatalogName GetCatalogName() override;
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

  void SliderButtonPressed();

 private:
  // Records when the user changes the gain via slider to metrics.
  void RecordGainChanged();

  // Timer used to prevent the input gain from recording each time the user
  // moves the slider while setting the desired volume.
  base::DelayTimer input_gain_metric_delay_timer_;

#if DCHECK_IS_ON()
  bool created_view_ = false;
#endif
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_MIC_GAIN_SLIDER_CONTROLLER_H_
