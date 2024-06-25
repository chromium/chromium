// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_slider_view.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"

namespace gfx {
class Insets;
}

namespace ash {
class UnifiedVolumeView;

// Controller of a slider that can change audio volume.
class ASH_EXPORT UnifiedVolumeSliderController : public UnifiedSliderListener {
 public:
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();
    virtual void OnAudioSettingsButtonClicked() = 0;

    base::WeakPtrFactory<Delegate> weak_ptr_factory_{this};
  };

  // This constructor is for controllers of `UnifiedVolumeView` with a trailing
  // settings button that leads to `AudioDetailedView`, i.e. volume slider in
  // the Quick Settings main page and Quick Settings toasts. `delegate` is used
  // to construct the callback for `more_button_`.
  explicit UnifiedVolumeSliderController(Delegate* delegate);

  // This constructor is for controllers of a single `UnifiedVolumeView`, i.e.
  // volume sliders in `AudioDetailedView`. `delegate_` is set to nullptr.
  UnifiedVolumeSliderController();

  UnifiedVolumeSliderController(const UnifiedVolumeSliderController&) = delete;
  UnifiedVolumeSliderController& operator=(
      const UnifiedVolumeSliderController&) = delete;

  ~UnifiedVolumeSliderController() override;

  // Creates a slider view for a specific output device in `AudioDetailedView`.
  std::unique_ptr<UnifiedVolumeView> CreateVolumeSlider(
      uint64_t device_id,
      const gfx::Insets& inside_padding = kRadioSliderViewPadding);

  // This callback is used to map the `device_id` to `UnifiedVolumeView` in
  // `UnifiedVolumeViewTest`.
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
  const raw_ptr<Delegate, DanglingUntriaged> delegate_;

  // Records when the user changes the output volume via slider to metrics.
  void RecordVolumeSourceMetric();

  // Timer used to prevent the input gain from recording each time the user
  // moves the slider while setting the desired volume.
  base::DelayTimer output_volume_metric_delay_timer_;

#if DCHECK_IS_ON()
  bool created_view_ = false;
#endif
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_
