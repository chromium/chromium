// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_BUBBLE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash {

class UnifiedSystemTray;
class UnifiedSliderListener;
class UnifiedSliderView;

// Controller class for independent slider bubbles e.g. volume slider and
// brightness slider that can be triggered from hardware buttons.
class ASH_EXPORT UnifiedSliderBubbleController
    : public TrayBubbleView::Delegate,
      public CrasAudioHandler::AudioObserver,
      public UnifiedSystemTrayModel::Observer,
      public UnifiedVolumeSliderController::Delegate,
      public ShelfObserver {
 public:
  enum SliderType {
    SLIDER_TYPE_VOLUME = 0,
    SLIDER_TYPE_DISPLAY_BRIGHTNESS,
    // TODO(b/298085976): Keyboard backlight sliders will migrate to toasts.
    SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE_OFF,
    SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE_ON,
    SLIDER_TYPE_KEYBOARD_BRIGHTNESS,
    SLIDER_TYPE_MIC
  };

  explicit UnifiedSliderBubbleController(UnifiedSystemTray* tray);

  UnifiedSliderBubbleController(const UnifiedSliderBubbleController&) = delete;
  UnifiedSliderBubbleController& operator=(
      const UnifiedSliderBubbleController&) = delete;

  ~UnifiedSliderBubbleController() override;

  // Show a slider of |slider_type|. If the slider of same type is already
  // shown, it just extends the auto close timer.
  void ShowBubble(SliderType slider_type);

  void CloseBubble();

  // True if a slider bubble is shown.
  bool IsBubbleShown() const;

  // Returns the height of the bubble. Used to calculate baseline offset for
  // notification popups or side aligned toasts.
  int GetBubbleHeight() const;

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  void OnMouseEnteredView() override;
  void OnMouseExitedView() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // Displays the microphone mute toast.
  void DisplayMicrophoneMuteToast();

  // CrasAudioHandler::AudioObserver:
  void OnInputMuteChanged(
      bool mute_on,
      CrasAudioHandler::InputMuteChangeMethod method) override;
  void OnInputMutedByMicrophoneMuteSwitchChanged(bool muted) override;
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override;
  void OnOutputMuteChanged(bool mute_on) override;

  // UnifiedSystemTrayModel::Observer:
  void OnDisplayBrightnessChanged(bool by_user) override;
  void OnKeyboardBrightnessChanged(
      power_manager::BacklightBrightnessChange_Cause cause) override;

  // UnifiedVolumeSliderController::Delegate:
  void OnAudioSettingsButtonClicked() override;

  // ShelfObserver:
  void OnShelfWorkAreaInsetsChanged() override;

  UnifiedSliderView* slider_view() { return slider_view_; }

 private:
  friend class UnifiedSystemTrayTest;

  // Instantiate |slider_controller_| of |slider_type_|.
  void CreateSliderController();

  // Start auto close timer.
  void StartAutoCloseTimer();

  // Unowned.
  const raw_ptr<UnifiedSystemTray> tray_;

  base::OneShotTimer autoclose_;

  raw_ptr<TrayBubbleView> bubble_view_ = nullptr;
  raw_ptr<views::Widget> bubble_widget_ = nullptr;
  raw_ptr<UnifiedSliderView> slider_view_ = nullptr;

  // Type of the currently shown slider.
  SliderType slider_type_ = SLIDER_TYPE_VOLUME;

  // Whether mouse is hovered on the bubble.
  bool mouse_hovered_ = false;

  // Controller of the current slider view. If a slider is not shown, it's null.
  // Owned.
  std::unique_ptr<UnifiedSliderListener> slider_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_BUBBLE_CONTROLLER_H_
