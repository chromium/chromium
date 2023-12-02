// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TRAY_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TRAY_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/tray/tray_background_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageButton;
}

namespace ash {

class FocusModeCountdownView;
class ProgressIndicator;
class TrayBubbleWrapper;

// Status area tray which is visible when focus mode is enabled. A circular
// progress bar is displayed around the tray displaying how much time is left in
// the focus session. The tray also controls a bubble that is shown when the
// button is clicked.
class ASH_EXPORT FocusModeTray : public TrayBackgroundView,
                                 public FocusModeController::Observer {
  METADATA_HEADER(FocusModeTray, TrayBackgroundView)

 public:
  explicit FocusModeTray(Shelf* shelf);
  FocusModeTray(const FocusModeTray&) = delete;
  FocusModeTray& operator=(const FocusModeTray&) = delete;
  ~FocusModeTray() override;

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
  void OnTimerTick() override;
  void OnSessionDurationChanged() override;

  TrayBubbleWrapper* tray_bubble_wrapper_for_testing() { return bubble_.get(); }
  FocusModeCountdownView* countdown_view_for_testing() {
    return countdown_view_;
  }
  const views::ImageButton* GetRadioButtonForTesting() const;
  const views::Label* GetTaskTitleForTesting() const;

 private:
  friend class FocusModeTrayTest;

  // TODO(b/314022131): Move `TaskItemView` to its own files.
  class TaskItemView;

  // Updates the image and color of the icon.
  void UpdateTrayIcon();

  // Button click handler for shelf icon.
  void FocusModeIconActivated(const ui::Event& event);

  // Calls `UpdateUI` on `countdown_view_` if it exists.
  void MaybeUpdateCountdownViewUI();

  // Called when the user clicks the radio button to mark a selected task as
  // completed.
  void OnCompleteTask();

  // Called when the animation in `AnimateBubbleResize` starts.
  void OnBubbleResizeAnimationStarted();
  // Called when the animation in `AnimateBubbleResize` ends.
  void OnBubbleResizeAnimationEnded();
  // Animates resizing the bubble view after `task_item_view_` has been removed
  // from the bubble.
  void AnimateBubbleResize();
  // Updates the progression of the progress indicator.
  void UpdateProgressRing();

  // Image view of the focus mode lamp.
  const raw_ptr<views::ImageView> image_view_;

  // The main content view of the bubble.
  raw_ptr<FocusModeCountdownView, DanglingUntriaged> countdown_view_ = nullptr;

  // A box layout view which has a radio/check icon and a label for a selected
  // task.
  raw_ptr<TaskItemView> task_item_view_ = nullptr;

  raw_ptr<views::BoxLayoutView> bubble_view_container_ = nullptr;

  // The bubble that appears after clicking the tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // An object that draws and updates the progress ring.
  std::unique_ptr<ProgressIndicator> progress_indicator_;

  base::WeakPtrFactory<FocusModeTray> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TRAY_H_
