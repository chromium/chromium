// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_BUBBLE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/timer/timer.h"
#include "chromeos/audio/cras_audio_handler.h"

namespace ash {

class UnifiedSystemTray;
class UnifiedSliderListener;

// Controller class for independent slider bubbles e.g. volume slider and
// brightness slider that can be triggered from hardware buttons.
class ASH_EXPORT UnifiedSliderBubbleController
    : public TrayBubbleView::Delegate,
      public chromeos::CrasAudioHandler::AudioObserver,
      public UnifiedSystemTrayModel::Observer,
      public UnifiedVolumeSliderController::Delegate {
 public:
  enum SliderType {
    SLIDER_TYPE_VOLUME = 0,
    SLIDER_TYPE_DISPLAY_BRIGHTNESS,
    SLIDER_TYPE_KEYBOARD_BRIGHTNESS
  };

  explicit UnifiedSliderBubbleController(UnifiedSystemTray* tray);
  ~UnifiedSliderBubbleController() override;

  // Show a slider of |slider_type|. If the slider of same type is already
  // shown, it just extends the auto close timer.
  void ShowBubble(SliderType slider_type);

  void CloseBubble();

  // True if a slider bubble is shown.
  bool IsBubbleShown() const;

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  void OnMouseEnteredView() override;
  void OnMouseExitedView() override;

  // chromeos::CrasAudioHandler::AudioObserver:
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override;
  void OnOutputMuteChanged(bool mute_on) override;

  // UnifiedSystemTrayModel::Observer:
  void OnDisplayBrightnessChanged(bool by_user) override;
  void OnKeyboardBrightnessChanged(bool by_user) override;

  // UnifiedVolumeSliderController::Delegate:
  void OnAudioSettingsButtonClicked() override;

 private:
  friend class UnifiedSystemTrayTest;

  // Instantiate |slider_controller_| of |slider_type_|.
  void CreateSliderController();

  // Start auto close timer.
  void StartAutoCloseTimer();

  // Unowned.
  UnifiedSystemTray* const tray_;

  base::OneShotTimer autoclose_;

  TrayBubbleView* bubble_view_ = nullptr;
  views::Widget* bubble_widget_ = nullptr;

  // Type of the currently shown slider.
  SliderType slider_type_ = SLIDER_TYPE_VOLUME;

  // Whether mouse is hovered on the bubble.
  bool mouse_hovered_ = false;

  // Controller of the current slider view. If a slider is not shown, it's null.
  // Owned.
  std::unique_ptr<UnifiedSliderListener> slider_controller_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSliderBubbleController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_BUBBLE_CONTROLLER_H_
