// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_screen/privacy_screen_toast_controller.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

PrivacyScreenToastController::PrivacyScreenToastController(
    UnifiedSystemTray* tray)
    : tray_(tray) {
  Shell::Get()->privacy_screen_controller()->AddObserver(this);
}

PrivacyScreenToastController::~PrivacyScreenToastController() {
  close_timer_.Stop();
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();
  Shell::Get()->privacy_screen_controller()->RemoveObserver(this);
}

void PrivacyScreenToastController::ShowToast() {
  // If the bubble already exists, update the content of the bubble and extend
  // the autoclose timer.
  if (bubble_widget_) {
    UpdateToastView();
    if (!mouse_hovered_)
      StartAutoCloseTimer();
    return;
  }

  tray_->CloseSecondaryBubbles();

  TrayBubbleView::InitParams init_params =
      CreateInitParamsForTrayBubble(tray_, /*anchor_to_shelf_corner=*/true);
  init_params.type = TrayBubbleView::TrayBubbleType::kSecondaryBubble;
  init_params.preferred_width = kPrivacyScreenToastMinWidth;

  // Use this controller as the delegate rather than the tray.
  init_params.delegate = GetWeakPtr();

  bubble_view_ = new TrayBubbleView(init_params);
  toast_view_ = new PrivacyScreenToastView(
      this, base::BindRepeating(&PrivacyScreenToastController::ButtonPressed,
                                base::Unretained(this)));
  bubble_view_->AddChildViewRaw(toast_view_.get());

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);

  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  StartAutoCloseTimer();
  UpdateToastView();

  tray_->NotifySecondaryBubbleHeight(toast_view_->height());

  // Activate the bubble so ChromeVox can announce the toast.
  if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    bubble_widget_->widget_delegate()->SetCanActivate(true);
    bubble_widget_->Activate();
  }
}

void PrivacyScreenToastController::HideToast() {
  close_timer_.Stop();
  if (!bubble_widget_ || bubble_widget_->IsClosed())
    return;
  bubble_widget_->Close();
  tray_->NotifySecondaryBubbleHeight(0);
}

void PrivacyScreenToastController::BubbleViewDestroyed() {
  close_timer_.Stop();
  toast_view_ = nullptr;
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
}

void PrivacyScreenToastController::OnMouseEnteredView() {
  close_timer_.Stop();
  mouse_hovered_ = true;
}

void PrivacyScreenToastController::OnMouseExitedView() {
  StartAutoCloseTimer();
  mouse_hovered_ = false;
}

std::u16string PrivacyScreenToastController::GetAccessibleNameForBubble() {
  return CalculateAccessibleNameForBubble();
}

void PrivacyScreenToastController::HideBubble(
    const TrayBubbleView* bubble_view) {}

void PrivacyScreenToastController::OnPrivacyScreenSettingChanged(
    bool enabled,
    bool notify_ui) {
  if (!notify_ui)
    return;

  if (tray_->IsBubbleShown())
    return;

  ShowToast();
}

void PrivacyScreenToastController::StartAutoCloseTimer() {
  close_timer_.Stop();

  // Don't start the timer if the toast is focused.
  if (toast_view_ && toast_view_->IsButtonFocused())
    return;

  close_timer_.Start(
      FROM_HERE,
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled()
          ? kSecondaryBubbleWithSpokenFeedbackDuration
          : kSecondaryBubbleDuration,
      this, &PrivacyScreenToastController::HideToast);
}

// static
std::u16string
PrivacyScreenToastController::CalculateAccessibleNameForBubble() {
  auto* privacy_screen_controller = Shell::Get()->privacy_screen_controller();
  std::u16string enabled_state = l10n_util::GetStringUTF16(
      privacy_screen_controller->GetEnabled()
          ? IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ON_STATE
          : IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_OFF_STATE);

  std::u16string managed_state =
      privacy_screen_controller->IsManaged()
          ? l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ENTERPRISE_MANAGED)
          : std::u16string();

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_TOAST_ACCESSIBILITY_TEXT,
      enabled_state, managed_state);
}

void PrivacyScreenToastController::UpdateToastView() {
  if (toast_view_) {
    auto* privacy_screen_controller = Shell::Get()->privacy_screen_controller();
    toast_view_->SetPrivacyScreenEnabled(
        /*enabled=*/privacy_screen_controller->GetEnabled(),
        /*managed=*/privacy_screen_controller->IsManaged());
    bubble_view_->UpdateAccessibleName();
    int width =
        std::clamp(toast_view_->GetPreferredSize().width(),
                   kPrivacyScreenToastMinWidth, kPrivacyScreenToastMaxWidth);
    bubble_view_->SetPreferredWidth(width);
  }
}

void PrivacyScreenToastController::ButtonPressed() {
  auto* privacy_screen_controller = Shell::Get()->privacy_screen_controller();
  privacy_screen_controller->SetEnabled(
      !privacy_screen_controller->GetEnabled());
}

void PrivacyScreenToastController::StopAutocloseTimer() {
  close_timer_.Stop();
}

}  // namespace ash
