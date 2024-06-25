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
#include "ash/system/unified/unified_slider_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "base/functional/bind.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"

namespace ash {

namespace {

using SliderType = UnifiedSliderBubbleController::SliderType;

// The padding of slider toast.
constexpr auto kQsSliderToastPadding = gfx::Insets::TLBR(8, 8, 8, 12);
constexpr auto kQsToggleToastPadding = gfx::Insets(12);
// The rounded corner radius of the `bubble_view_`.
constexpr int kQsToastCornerRadius = 28;

// Return true if a system tray bubble is shown in any display.
bool IsAnyMainBubbleShown() {
  for (RootWindowController* root : Shell::GetAllRootWindowControllers()) {
    if (root->GetStatusAreaWidget()->unified_system_tray()->IsBubbleShown()) {
      return true;
    }
  }
  return false;
}

void ConfigureSliderViewStyle(UnifiedSliderView* slider_view,
                              SliderType slider_type) {
  // Toggle toast has only a button and label. Slider toast has a slider, a
  // button on the slider body, and possible trailing buttons.
  const bool is_toggle_toast =
      slider_type == SliderType::SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE_ON ||
      slider_type == SliderType::SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE_OFF;
  auto* layout =
      slider_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          is_toggle_toast ? kQsToggleToastPadding : kQsSliderToastPadding,
          kSliderChildrenViewSpacing));
  layout->SetFlexForView(slider_view->slider(), /*flex=*/1);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
}

// Returns whether the `VideoConferenceTray` should be shown.
bool ShouldVideoConferenceTrayBeShown() {
  if (!features::IsVideoConferenceEnabled()) {
    return false;
  }

  // The tray is shown on every display, so just check the primary display.
  auto* status_area_widget =
      Shell::Get()->GetPrimaryRootWindowController()->GetStatusAreaWidget();
  // `UnifiedSliderBubbleController` belongs to `UnifiedSystemTray` which is
  // created during construction of `status_area_widget`, so it is possible
  // this is called before `status_area_widget` is created.
  return status_area_widget &&
         status_area_widget->video_conference_tray()->visible_preferred();
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
  if (bubble_widget_) {
    // Reset `slider_view_`
    // to prevent dangling pointer caused by view removal.
    // TODO(b/40280409): We shouldn't need this if child view removal is made
    // more safe.
    slider_view_ = nullptr;

    bubble_widget_->CloseNow();
  }
}

void UnifiedSliderBubbleController::CloseBubble() {
  autoclose_.Stop();
  slider_controller_.reset();
  if (!bubble_widget_) {
    return;
  }
  // Ignore the request if the bubble is closing.
  if (bubble_widget_->IsClosed()) {
    return;
  }
  bubble_widget_->Close();
  tray_->NotifySecondaryBubbleHeight(0);
}

bool UnifiedSliderBubbleController::IsBubbleShown() const {
  return !!bubble_widget_ && !bubble_widget_->IsClosed();
}

int UnifiedSliderBubbleController::GetBubbleHeight() const {
  return !!slider_view_ ? slider_view_->height() : 0;
}

void UnifiedSliderBubbleController::BubbleViewDestroyed() {
  slider_controller_.reset();
  slider_view_ = nullptr;
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

void UnifiedSliderBubbleController::HideBubble(
    const TrayBubbleView* bubble_view) {}

void UnifiedSliderBubbleController::DisplayMicrophoneMuteToast() {
  // We will not display the microphone mute toast if no microphone is connected
  // to the device, or if the video conference controls tray is visible.
  if (CrasAudioHandler::Get()->HasActiveInputDeviceForSimpleUsage() &&
      !ShouldVideoConferenceTrayBeShown()) {
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
  if (by_user) {
    ShowBubble(SLIDER_TYPE_DISPLAY_BRIGHTNESS);
  }
}

void UnifiedSliderBubbleController::OnKeyboardBrightnessChanged(
    power_manager::BacklightBrightnessChange_Cause cause) {
  if (cause == power_manager::BacklightBrightnessChange_Cause_USER_REQUEST) {
    // User has made a brightness adjustment, or the KBL was made
    // no-longer-forced-off implicitly in response to a user adjustment.
    ShowBubble(SLIDER_TYPE_KEYBOARD_BRIGHTNESS);
    if (Shell::Get()->rgb_keyboard_manager()->IsRgbKeyboardSupported()) {
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
    ShowBubble((cause ==
                power_manager::BacklightBrightnessChange_Cause_USER_TOGGLED_OFF)
                   ? SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE_OFF
                   : SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE_ON);
  }
}

void UnifiedSliderBubbleController::OnAudioSettingsButtonClicked() {
  tray_->ShowAudioDetailedViewBubble();
}

void UnifiedSliderBubbleController::OnShelfWorkAreaInsetsChanged() {
  if (bubble_view_) {
    bubble_view_->ChangeAnchorRect(tray_->shelf()->GetSystemTrayAnchorRect());
  }
}

void UnifiedSliderBubbleController::ShowBubble(SliderType slider_type) {
  // Never show slider bubble in kiosk app mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode()) {
    return;
  }

  bool is_audio_slider = slider_type == SLIDER_TYPE_MIC;
  // both the volume slider and mic gain slider will be shown in
  // `AudioDetailedView`.
  is_audio_slider = is_audio_slider || slider_type == SLIDER_TYPE_VOLUME;

  // When tray bubble is already shown, the microphone slider will get shown in
  // audio detailed view. Bail out if the audio details are already showing to
  // avoid resetting the bubble state.
  // If already in the `AudioDetailedView`, bail out if it's either
  // `SLIDER_TYPE_MIC` or `SLIDER_TYPE_VOLUME`.
  if (is_audio_slider && tray_->bubble() &&
      tray_->bubble()->ShowingAudioDetailedView()) {
    return;
  }

  // When tray bubble is already shown, the brightness slider will get shown in
  // display detailed view. Bail out if the display details are already showing
  // to avoid resetting the bubble state.
  if (slider_type == SLIDER_TYPE_DISPLAY_BRIGHTNESS && tray_->bubble() &&
      tray_->bubble()->ShowingDisplayDetailedView()) {
    return;
  }

  if (IsAnyMainBubbleShown()) {
    // If a detailed view is showing, first transit to the main view.
    if (tray_->bubble() && tray_->bubble()->GetBubbleWidget()) {
      tray_->bubble()->unified_system_tray_controller()->TransitionToMainView(
          false);
    }

    // Unlike VOLUME and BRIGHTNESS, which are shown in the main bubble view,
    // MIC slider is shown in the audio details view.
    if (slider_type == SLIDER_TYPE_MIC && tray_->bubble()) {
      tray_->ShowAudioDetailedViewBubble();
    }
    return;
  }

  // Ignore the request if the bubble is closing.
  if (bubble_widget_ && bubble_widget_->IsClosed()) {
    return;
  }

  // If the bubble already exists, update the content of the bubble and extend
  // the autoclose timer.
  if (bubble_widget_) {
    CHECK(bubble_view_);

    if (slider_type_ != slider_type) {
      slider_type_ = slider_type;

      // Recreate the slider controller first to prevent dangling pointers when
      // removing child views.
      CreateSliderController();

      // `RemoveAllChildViews` will cause `slider_view_` to be dangling, so we
      // need to safely extract it.
      // TODO(b/40280409): We shouldn't need this if child view removal is made
      // more safe.
      slider_view_ = nullptr;
      bubble_view_->RemoveAllChildViews();

      slider_view_ = static_cast<UnifiedSliderView*>(
          bubble_view_->AddChildView(slider_controller_->CreateView()));
      ConfigureSliderViewStyle(slider_view_, slider_type);
      bubble_view_->DeprecatedLayoutImmediately();
    }

    // If mouse is hovered, do not restart auto close timer.
    if (!mouse_hovered_) {
      StartAutoCloseTimer();
    }
    return;
  }

  tray_->CloseSecondaryBubbles();

  CHECK(!bubble_view_);

  slider_type_ = slider_type;
  CreateSliderController();

  TrayBubbleView::InitParams init_params =
      CreateInitParamsForTrayBubble(tray_, /*anchor_to_shelf_corner=*/true);
  init_params.type = TrayBubbleView::TrayBubbleType::kSecondaryBubble;
  init_params.reroute_event_handler = false;

  // Use this controller as the delegate rather than the tray.
  init_params.delegate = GetWeakPtr();

  init_params.corner_radius = kQsToastCornerRadius;
  // `bubble_view_` is fully rounded, so sets it to be true and paints the
  // shadow on texture layer.
  init_params.has_large_corner_radius = true;

  bubble_view_ = new TrayBubbleView(init_params);
  bubble_view_->SetCanActivate(false);
  slider_view_ = static_cast<UnifiedSliderView*>(
      bubble_view_->AddChildView(slider_controller_->CreateView()));
  ConfigureSliderViewStyle(slider_view_, slider_type);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);

  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  // Notify value change accessibility event because the popup is triggered by
  // changing value using an accessor key like VolUp.
  slider_view_->slider()->NotifyAccessibilityEvent(
      ax::mojom::Event::kValueChanged, true);

  StartAutoCloseTimer();

  tray_->NotifySecondaryBubbleHeight(slider_view_->height());
}

void UnifiedSliderBubbleController::CreateSliderController() {
  switch (slider_type_) {
    case SLIDER_TYPE_VOLUME:
      slider_controller_ =
          std::make_unique<UnifiedVolumeSliderController>(this);
      return;
    case SLIDER_TYPE_DISPLAY_BRIGHTNESS:
      slider_controller_ = std::make_unique<UnifiedBrightnessSliderController>(
          tray_->model().get(),
          base::BindRepeating(&UnifiedSystemTray::ShowDisplayDetailedViewBubble,
                              base::Unretained(tray_)));
      return;
    case SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE_OFF:
      slider_controller_ = std::make_unique<KeyboardBacklightToggleController>(
          tray_->model().get(), /*toggled_on=*/false);
      return;
    case SLIDER_TYPE_KEYBOARD_BACKLIGHT_TOGGLE_ON:
      slider_controller_ = std::make_unique<KeyboardBacklightToggleController>(
          tray_->model().get(), /*toggled_on=*/true);
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
  autoclose_.Start(FROM_HERE, kSecondaryBubbleDuration, this,
                   &UnifiedSliderBubbleController::CloseBubble);
}

}  // namespace ash
