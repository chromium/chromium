// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/autozoom_toast_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/camera/autozoom_controller_impl.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/cxx17_backports.h"

namespace ash {

AutozoomToastController::Delegate::Delegate() = default;

void AutozoomToastController::Delegate::AddAutozoomObserver(
    AutozoomObserver* observer) {
  Shell::Get()->autozoom_controller()->AddObserver(observer);
}

void AutozoomToastController::Delegate::RemoveAutozoomObserver(
    AutozoomObserver* observer) {
  Shell::Get()->autozoom_controller()->RemoveObserver(observer);
}

bool AutozoomToastController::Delegate::IsAutozoomEnabled() {
  return Shell::Get()->autozoom_controller()->GetState() !=
         cros::mojom::CameraAutoFramingState::OFF;
}

bool AutozoomToastController::Delegate::IsAutozoomControlEnabled() {
  return Shell::Get()->autozoom_controller()->IsAutozoomControlEnabled();
}

AutozoomToastController::AutozoomToastController(
    UnifiedSystemTray* tray,
    std::unique_ptr<Delegate> delegate)
    : tray_(tray), delegate_(std::move(delegate)) {
  delegate_->AddAutozoomObserver(this);
}

AutozoomToastController::~AutozoomToastController() {
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();
  delegate_->RemoveAutozoomObserver(this);
}

void AutozoomToastController::ShowToast() {
  // If the bubble already exists, update the content of the bubble and extend
  // the autoclose timer.
  if (bubble_widget_) {
    UpdateToastView();
    if (!mouse_hovered_)
      StartAutoCloseTimer();
    return;
  }

  tray_->CloseSecondaryBubbles();

  TrayBubbleView::InitParams init_params;
  init_params.shelf_alignment = tray_->shelf()->alignment();
  init_params.preferred_width = kAutozoomToastMinWidth;
  init_params.delegate = GetWeakPtr();
  init_params.parent_window = tray_->GetBubbleWindowContainer();
  init_params.anchor_view = nullptr;
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = tray_->shelf()->GetSystemTrayAnchorRect();
  // Decrease bottom and right insets to compensate for the adjustment of
  // the respective edges in Shelf::GetSystemTrayAnchorRect().
  init_params.insets = GetTrayBubbleInsets();
  init_params.translucent = true;

  auto bubble_view = std::make_unique<TrayBubbleView>(init_params);
  // bubble_view_ is owned by the view hierarchy and not by this class.
  bubble_view_ = bubble_view.get();
  toast_view_ =
      bubble_view->AddChildView(std::make_unique<AutozoomToastView>(this));

  bubble_widget_ =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));

  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  StartAutoCloseTimer();
  UpdateToastView();

  tray_->SetTrayBubbleHeight(
      bubble_widget_->GetWindowBoundsInScreen().height());
}

void AutozoomToastController::HideToast() {
  close_timer_.Stop();
  if (!bubble_widget_ || bubble_widget_->IsClosed())
    return;
  bubble_widget_->Close();
  tray_->SetTrayBubbleHeight(0);
}

void AutozoomToastController::BubbleViewDestroyed() {
  close_timer_.Stop();
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
}

void AutozoomToastController::OnMouseEnteredView() {
  close_timer_.Stop();
  mouse_hovered_ = true;
}

void AutozoomToastController::OnMouseExitedView() {
  StartAutoCloseTimer();
  mouse_hovered_ = false;
}

std::u16string AutozoomToastController::GetAccessibleNameForBubble() {
  if (!toast_view_)
    return std::u16string();
  return toast_view_->GetAccessibleName();
}

void AutozoomToastController::StartAutoCloseTimer() {
  close_timer_.Stop();

  // Don't start the timer if the toast is focused.
  if (toast_view_ && toast_view_->IsButtonFocused())
    return;

  int autoclose_delay = kTrayPopupAutoCloseDelayInSeconds;
  if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled())
    autoclose_delay = kTrayPopupAutoCloseDelayInSecondsWithSpokenFeedback;

  close_timer_.Start(FROM_HERE, base::Seconds(autoclose_delay), this,
                     &AutozoomToastController::HideToast);
}

void AutozoomToastController::OnAutozoomStateChanged(
    cros::mojom::CameraAutoFramingState state) {
  if (state == cros::mojom::CameraAutoFramingState::OFF) {
    // TODO(pihsun): Should we hide toast immediately when the autozoom is
    // toggled off while the toast is showing? Or should there be a "Autozoom
    // is off" toast?
    HideToast();
  }
}

void AutozoomToastController::UpdateToastView() {
  if (toast_view_) {
    toast_view_->SetAutozoomEnabled(/*enabled=*/delegate_->IsAutozoomEnabled());
    int width = base::clamp(toast_view_->GetPreferredSize().width(),
                            kAutozoomToastMinWidth, kAutozoomToastMaxWidth);
    bubble_view_->SetPreferredWidth(width);
  }
}

void AutozoomToastController::StopAutocloseTimer() {
  close_timer_.Stop();
}

void AutozoomToastController::OnAutozoomControlEnabledChanged(bool enabled) {
  if (enabled) {
    if (delegate_->IsAutozoomEnabled()) {
      ShowToast();
    } else {
      HideToast();
    }
  }
}

}  // namespace ash
