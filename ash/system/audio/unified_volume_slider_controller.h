// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_

#include "ash/system/unified/unified_slider_view.h"

namespace ash {

// Controller of a slider that can change audio volume.
class UnifiedVolumeSliderController : public UnifiedSliderListener {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnAudioSettingsButtonClicked() = 0;
  };

  UnifiedVolumeSliderController(Delegate* delegate, bool in_bubble);
  ~UnifiedVolumeSliderController() override;

  // UnifiedSliderListener:
  views::View* CreateView() override;
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

  void SliderButtonPressed();

 private:
  Delegate* const delegate_;

  // Whether the volume slider is in the bubble, as opposed to the system tray.
  const bool in_bubble_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedVolumeSliderController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_
