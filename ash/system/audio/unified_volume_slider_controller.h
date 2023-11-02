// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/unified_slider_view.h"

namespace ash {

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

  explicit UnifiedVolumeSliderController(Delegate* delegate);

  UnifiedVolumeSliderController(const UnifiedVolumeSliderController&) = delete;
  UnifiedVolumeSliderController& operator=(
      const UnifiedVolumeSliderController&) = delete;

  ~UnifiedVolumeSliderController() override;

  // UnifiedSliderListener:
  views::View* CreateView() override;
  QsSliderCatalogName GetCatalogName() override;
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

  void SliderButtonPressed();

 private:
  Delegate* const delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_
