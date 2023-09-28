// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TRAY_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TRAY_H_

#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class TrayBubbleWrapper;

// Status area tray which is visible when focus mode is enabled. A circular
// progress bar is displayed around the tray displaying how much time is left in
// the focus session. The tray also controls a bubble that is shown when the
// button is clicked.
class FocusModeTray : public TrayBackgroundView,
                      public FocusModeController::Observer {
 public:
  explicit FocusModeTray(Shelf* shelf);
  FocusModeTray(const FocusModeTray&) = delete;
  FocusModeTray& operator=(const FocusModeTray&) = delete;
  ~FocusModeTray() override;

  TrayBubbleWrapper* tray_bubble_wrapper_for_testing() { return bubble_.get(); }

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void HideBubble(const TrayBubbleView* bubble_view) override;
  void CloseBubble() override;
  void ShowBubble() override;
  void UpdateTrayItemColor(bool is_active) override;
  void OnThemeChanged() override;

  // FocusModeController::Observer:
  void OnFocusModeChanged(bool in_focus_session) override;

 private:
  // Updates the image and color of the icon.
  void UpdateTrayIcon();

  // Button click handler for shelf icon.
  void FocusModeIconActivated(const ui::Event& event);

  // Image view of the focus mode lamp.
  const raw_ptr<views::ImageView> image_view_;

  // The bubble that appears after clicking the tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  base::WeakPtrFactory<FocusModeTray> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TRAY_H_
