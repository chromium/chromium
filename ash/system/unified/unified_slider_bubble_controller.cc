// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_slider_bubble_controller.h"

#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/audio/mic_gain_slider_controller.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"
#include "ash/system/keyboard_brightness/keyboard_backlight_color_nudge_controller.h"
#include "ash/system/keyboard_brightness/keyboard_backlight_toggle_controller.h"
#include "ash/system/keyboard_brightness/unified_keyboard_brightness_slider_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/views/border.h"

namespace ash {

namespace {

// Return true if a system tray bubble is shown in any display.
bool IsAnyMainBubbleShown() {
  for (RootWindowController* root : Shell::GetAllRootWindowControllers()) {
    if (root->GetStatusAreaWidget()->unified_system_tray()->IsBubbleShown())
      return true;
  }
  return false;
}

void ConfigureSliderViewStyle(views::View* slider_view) {
  slider_view->SetBorder(views::CreateEmptyBorder(kUnifiedSliderBubblePadding));
}

}  // namespace

UnifiedSliderBubbleController::UnifiedSliderBubbleController(
    UnifiedSystemTray* tray)
    : tray_(tray) {
  CrasAudioHandler::Get()->AddAudioObserver(this);
  tray_->model()->AddObserver(this);
  tray_->shelf()->AddObserver(this);
}

UnifiedSliderBubbleController::~UnifiedSliderBubbleController() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  tray_->model()->RemoveObserver(this);
  tray_->shelf()->RemoveObserver(this);
  autoclose_.Stop();
  slider_controller_.reset();
  if (bubble_widget_)
    bubble_widget_->CloseNow();
}

void UnifiedSliderBubbleController::CloseBubble() {
  autoclose_.Stop();
  slider_controller_.reset();
  if (!bubble_widget_)
    return;
  // Ignore the request if the bubble is closing.
  if (bubble_widget_->IsClosed())
    return;
  bubble_widget_->Close();
  tray_->SetTrayBubbleHeight(0);
}

bool UnifiedSliderBubbleController::IsBubbleShown() const {
  return !!bubble_widget_;
}

void UnifiedSliderBubbleController::BubbleViewDestroyed() {
  slider_controller_.reset();
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
}

void UnifiedSliderBubbleController::OnMouseEnteredView() {
  // If mouse if hovered, pause auto close timer until mouse moves out.
  autoclose_.Stop();
  mouse_hovered_ = true;
}

void UnifiedSliderBubbleController::OnMouseExitedView() {
  StartAutoCloseTimer();
  mouse_hovered_ = false;
}

void UnifiedSliderBubbleController::DisplayMicrophoneMuteToast() {
  // We will not display the microphone mute toast if no microphone is connected
  // to the device.
  if (features::IsMicMuteNotificationsEnabled() &&
      CrasAudioHandler::Get()->HasActiveInputDeviceForSimpleUsage()) {
    ShowBubble(SLIDER_TYPE_MIC);
  }
}

void UnifiedSliderBubbleController::OnInputMuteChanged(
    bool mute,
    CrasAudioHandler::InputMuteChangeMethod method) {
  // The toast is displayed when the input mute state is changed by the
  // dedicated keyboard button.
  if (method == CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton) {
    DisplayMicrophoneMuteToast();
  }
}

void UnifiedSliderBubbleController::OnInputMutedByMicrophoneMuteSwitchChanged(
    bool muted) {
  // The toast is displayed whenever the state of the hadrdware switch changes.
  DisplayMicrophoneMuteToast();
}

void UnifiedSliderBubbleController::OnOutputNodeVolumeChanged(uint64_t node_id,
                                                              int volume) {
  ShowBubble(SLIDER_TYPE_VOLUME);
}

void UnifiedSliderBubbleController::OnOutputMuteChanged(bool mute_on) {
  ShowBubble(SLIDER_TYPE_VOLUME);
}

void UnifiedSliderBubbleController::OnDisplayBrightnessChanged(bool by_user) {
  if (by_user)
    ShowBubble(SLIDER_TYPE_DISPLAY_BRIGHTNESS);
}

void UnifiedSliderBubbleController::OnKeyboardBrightnessChanged(
    power_manager::BacklightBrightnessChange_Cause cause) {
  if (cause == power_manager::BacklightBrightnessChange_Cause_USER_REQUEST) {
    // User has made a brightness adjustment, or the KBL was made
    // no-longer-forced-off implicitly in response to a user adjustment.
    ShowBubble(SLIDER_TYPE_KEYBOARD_BRIGHTNESS);
    if (features::IsRgbKeyboardEnabled() &&
        Shell::Get()->rgb_keyboard_manager()->IsRgbKeyboardSupported()) {
      // Show the education nudge to change the keyboard backlight color if
      // applicable. |bubble_view_| is used as the anchor view.
      Shell::Get()
          ->keyboard_backlight_color_controller()
          ->keyboard_backlight_color_nudge_controller()
          ->MaybeShowEducationNudge(bubble_view_);
    }
  } else if (cause == power_manager::
                          BacklightBrightnessChange_Cause_USER_TOGGLED_OFF ||
             cause == power_manager::
                          BacklightBrightnessChange_Cause_USER_TOGGLED_ON) {
    // User has explicitly toggled the KBL backlight.
    ShowBubble(SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE);
  }
}

void UnifiedSliderBubbleController::OnAudioSettingsButtonClicked() {
  tray_->ShowAudioDetailedViewBubble();
}

void UnifiedSliderBubbleController::OnShelfWorkAreaInsetsChanged() {
  if (bubble_view_)
    bubble_view_->ChangeAnchorRect(tray_->shelf()->GetSystemTrayAnchorRect());
}

void UnifiedSliderBubbleController::ShowBubble(SliderType slider_type) {
  // Never show slider bubble in kiosk app mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode())
    return;

  // When tray bubble is already shown, the microphone slider will get shown in
  // audio detailed view. Bail out if the audio details are already showing to
  // avoid resetting the bubble state.
  if (slider_type == SLIDER_TYPE_MIC && tray_->bubble() &&
      tray_->bubble()->ShowingAudioDetailedView()) {
    return;
  }

  if (IsAnyMainBubbleShown()) {
    tray_->EnsureBubbleExpanded();

    // Unlike VOLUME and BRIGHTNESS, which are shown in the main bubble view,
    // MIC slider is shown in the audio details view.
    if (slider_type == SLIDER_TYPE_MIC && tray_->bubble())
      tray_->ShowAudioDetailedViewBubble();
    return;
  }

  // Ignore the request if the bubble is closing.
  if (bubble_widget_ && bubble_widget_->IsClosed())
    return;

  // If the bubble already exists, update the content of the bubble and extend
  // the autoclose timer.
  if (bubble_widget_) {
    DCHECK(bubble_view_);

    if (slider_type_ != slider_type) {
      bubble_view_->RemoveAllChildViews();

      slider_type_ = slider_type;
      CreateSliderController();

      UnifiedSliderView* slider_view =
          static_cast<UnifiedSliderView*>(slider_controller_->CreateView());
      ConfigureSliderViewStyle(slider_view);
      bubble_view_->AddChildView(slider_view);
      bubble_view_->Layout();
    }

    // If mouse is hovered, do not restart auto close timer.
    if (!mouse_hovered_)
      StartAutoCloseTimer();
    return;
  }

  tray_->CloseSecondaryBubbles();

  DCHECK(!bubble_view_);

  slider_type_ = slider_type;
  CreateSliderController();

  TrayBubbleView::InitParams init_params;

  init_params.shelf_alignment = tray_->shelf()->alignment();
  init_params.preferred_width = kTrayMenuWidth;
  init_params.delegate = GetWeakPtr();
  init_params.parent_window = tray_->GetBubbleWindowContainer();
  init_params.anchor_view = nullptr;
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = tray_->shelf()->GetSystemTrayAnchorRect();
  // Decrease bottom and right insets to compensate for the adjustment of
  // the respective edges in Shelf::GetSystemTrayAnchorRect().
  init_params.insets = GetTrayBubbleInsets();
  init_params.translucent = true;

  bubble_view_ = new TrayBubbleView(init_params);
  bubble_view_->SetCanActivate(false);
  UnifiedSliderView* slider_view =
      static_cast<UnifiedSliderView*>(slider_controller_->CreateView());
  ConfigureSliderViewStyle(slider_view);
  bubble_view_->AddChildView(slider_view);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);

  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  // Notify value change accessibility event because the popup is triggered by
  // changing value using an accessor key like VolUp.
  slider_view->slider()->NotifyAccessibilityEvent(
      ax::mojom::Event::kValueChanged, true);

  StartAutoCloseTimer();

  tray_->SetTrayBubbleHeight(
      bubble_widget_->GetWindowBoundsInScreen().height());
}

void UnifiedSliderBubbleController::CreateSliderController() {
  switch (slider_type_) {
    case SLIDER_TYPE_VOLUME:
      slider_controller_ =
          std::make_unique<UnifiedVolumeSliderController>(this);
      return;
    case SLIDER_TYPE_DISPLAY_BRIGHTNESS:
      slider_controller_ = std::make_unique<UnifiedBrightnessSliderController>(
          tray_->model().get());
      return;
    case SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE:
      slider_controller_ = std::make_unique<KeyboardBacklightToggleController>(
          tray_->model().get());
      return;
    case SLIDER_TYPE_KEYBOARD_BRIGHTNESS:
      slider_controller_ =
          std::make_unique<UnifiedKeyboardBrightnessSliderController>(
              tray_->model().get());
      return;
    case SLIDER_TYPE_MIC:
      slider_controller_ = std::make_unique<MicGainSliderController>();
      return;
  }
}

void UnifiedSliderBubbleController::StartAutoCloseTimer() {
  autoclose_.Stop();
  autoclose_.Start(FROM_HERE, base::Seconds(kTrayPopupAutoCloseDelayInSeconds),
                   this, &UnifiedSliderBubbleController::CloseBubble);
}

}  // namespace ash
