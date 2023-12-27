// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAMERA_AUTOZOOM_TOAST_CONTROLLER_H_
#define ASH_SYSTEM_CAMERA_AUTOZOOM_TOAST_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/camera/autozoom_observer.h"
#include "ash/system/camera/autozoom_toast_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"

namespace ash {

class UnifiedSystemTray;

// Controller class for the autozoom toast, which is shown when the autozoom is
// on and camera is opened.
class ASH_EXPORT AutozoomToastController : public TrayBubbleView::Delegate,
                                           public AutozoomObserver {
 public:
  // The Delegate interface handles adding and removing observers on behalf of
  // AutozoomToastController. This is used for unit tests.
  class ASH_EXPORT Delegate {
   public:
    Delegate();

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual void AddAutozoomObserver(AutozoomObserver* observer);

    virtual void RemoveAutozoomObserver(AutozoomObserver* observer);

    virtual bool IsAutozoomEnabled();

    virtual bool IsAutozoomControlEnabled();
  };

  AutozoomToastController(UnifiedSystemTray* tray,
                          std::unique_ptr<Delegate> delegate);
  AutozoomToastController(AutozoomToastController&) = delete;
  AutozoomToastController operator=(AutozoomToastController&) = delete;
  ~AutozoomToastController() override;

  // Shows the toast explicitly. Normally this is shown when there's a new
  // active camera client and autozoom is enabled.
  void ShowToast();

  // Hides the toast if it is shown. Normally, it times out and automatically
  // closes.
  void HideToast();

  // Stops the timer to autoclose the toast.
  void StopAutocloseTimer();

  // Triggers a timer to automatically close the toast.
  void StartAutoCloseTimer();

 protected:
  views::Widget* bubble_widget_for_test() { return bubble_widget_; }

 private:
  friend class AutozoomToastControllerTest;

  // AutozoomObserver:
  void OnAutozoomStateChanged(
      cros::mojom::CameraAutoFramingState state) override;
  void OnAutozoomControlEnabledChanged(bool enabled) override;

  // Updates the toast UI with the current privacy screen state.
  void UpdateToastView();

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  void OnMouseEnteredView() override;
  void OnMouseExitedView() override;
  std::u16string GetAccessibleNameForBubble() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  const raw_ptr<UnifiedSystemTray> tray_;
  raw_ptr<TrayBubbleView> bubble_view_ = nullptr;
  raw_ptr<views::Widget> bubble_widget_ = nullptr;
  raw_ptr<AutozoomToastView, DanglingUntriaged> toast_view_ = nullptr;
  bool mouse_hovered_ = false;
  base::OneShotTimer close_timer_;

  const std::unique_ptr<Delegate> delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAMERA_AUTOZOOM_TOAST_CONTROLLER_H_
