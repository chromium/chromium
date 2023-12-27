// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_TOAST_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_TOAST_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/system/privacy_screen/privacy_screen_toast_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"

namespace ash {

class UnifiedSystemTray;

// Controller class for the privacy screen toast, which is shown when the
// privacy screen is toggled on/off.
class ASH_EXPORT PrivacyScreenToastController
    : public TrayBubbleView::Delegate,
      public PrivacyScreenController::Observer {
 public:
  explicit PrivacyScreenToastController(UnifiedSystemTray* tray);
  ~PrivacyScreenToastController() override;
  PrivacyScreenToastController(PrivacyScreenToastController&) = delete;
  PrivacyScreenToastController operator=(PrivacyScreenToastController&) =
      delete;

  // Shows the toast explicitly. Normally, this is done automatically through
  // the PrivacyScreenToastController observer in this class.
  void ShowToast();

  // Hides the toast if it is shown. Normally, it times out and automatically
  // closes.
  void HideToast();

  // Stops the timer to autoclose the toast.
  void StopAutocloseTimer();

  // Triggers a timer to automatically close the toast.
  void StartAutoCloseTimer();

 private:
  // Updates the toast UI with the current privacy screen state.
  void UpdateToastView();

  void ButtonPressed();

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  void OnMouseEnteredView() override;
  void OnMouseExitedView() override;
  std::u16string GetAccessibleNameForBubble() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // PrivacyScreenController::Observer:
  void OnPrivacyScreenSettingChanged(bool enabled, bool notify_ui) override;

  const raw_ptr<UnifiedSystemTray> tray_;
  raw_ptr<TrayBubbleView> bubble_view_ = nullptr;
  raw_ptr<views::Widget> bubble_widget_ = nullptr;
  raw_ptr<PrivacyScreenToastView, DanglingUntriaged> toast_view_ = nullptr;
  bool mouse_hovered_ = false;
  base::OneShotTimer close_timer_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_TOAST_CONTROLLER_H_
