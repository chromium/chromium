// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TRAY_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TRAY_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_countdown_view.h"
#include "ash/system/focus_mode/focus_mode_ending_moment_view.h"
#include "ash/system/focus_mode/focus_mode_tasks_model.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageButton;
}

namespace ash {

class ProgressIndicator;
class TrayBubbleWrapper;

// Status area tray which is visible when focus mode is enabled. A circular
// progress bar is displayed around the tray displaying how much time is left in
// the focus session. The tray also controls a bubble that is shown when the
// button is clicked.
class ASH_EXPORT FocusModeTray : public TrayBackgroundView,
                                 public FocusModeController::Observer,
                                 public FocusModeTasksModel::Observer {
  METADATA_HEADER(FocusModeTray, TrayBackgroundView)

 public:
  explicit FocusModeTray(Shelf* shelf);
  FocusModeTray(const FocusModeTray&) = delete;
  FocusModeTray& operator=(const FocusModeTray&) = delete;
  ~FocusModeTray() override;

  // TrayBackgroundView:
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  std::u16string GetAccessibleNameForTray() override;
  std::u16string GetAccessibleNameForBubble() override;
  void HandleLocaleChange() override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void HideBubble(const TrayBubbleView* bubble_view) override;
  TrayBubbleView* GetBubbleView() override;
  void CloseBubbleInternal() override;
  void ShowBubble() override;
  void UpdateTrayItemColor(bool is_active) override;
  void OnThemeChanged() override;
  void OnAnimationEnded() override;

  // FocusModeController::Observer:
  void OnFocusModeChanged(FocusModeSession::State session_state) override;
  void OnTimerTick(const FocusModeSession::Snapshot& session_snapshot) override;
  void OnActiveSessionDurationChanged(
      const FocusModeSession::Snapshot& session_snapshot) override;

  // FocusModeTasksModel::Observer:
  void OnSelectedTaskChanged(const std::optional<FocusModeTask>& task) override;
  void OnTasksUpdated(const std::vector<FocusModeTask>& tasks) override;
  void OnTaskCompleted(const FocusModeTask& completed_task) override;

  // views::View:
  void Layout(PassKey) override;

  views::ImageView* image_view() { return image_view_; }

  // Triggers the tray bounce in animation. This is used during the ending
  // moment to notify the user that their session is over. When the animation
  // finishes, the ending moment nudge is then shown.
  void MaybePlayBounceInAnimation();

  FocusModeCountdownView* countdown_view_for_testing() {
    return countdown_view_;
  }
  FocusModeEndingMomentView* ending_moment_view_for_testing() {
    return ending_moment_view_;
  }
  const views::ImageButton* GetRadioButtonForTesting() const;
  const views::Label* GetTaskTitleForTesting() const;

 private:
  friend class FocusModeTrayTest;

  // TODO(b/314022131): Move `TaskItemView` to its own files.
  class TaskItemView;

  // Helper function for creating and setting up the `TaskItemView`.
  void CreateTaskItemView(const std::string& task_title);

  // Updates the image and color of the icon.
  void UpdateTrayIcon();

  // Button click handler for shelf icon.
  void FocusModeIconActivated(const ui::Event& event);

  void UpdateBubbleViews(const FocusModeSession::Snapshot& session_snapshot);

  // Calls `UpdateUI` on `countdown_view_` if it exists and is visible.
  void MaybeUpdateCountdownViewUI(
      const FocusModeSession::Snapshot& session_snapshot);

  // Calls `UpdateUI` on `ending_moment_view_` if it exists and is visible.
  void MaybeUpdateEndingMomentViewUI(
      const FocusModeSession::Snapshot& session_snapshot);

  // Called when the user clicks the radio button to mark a selected task as
  // completed, or if the task is already completed when we show the bubble.
  // `update` is used to determine if we need to update the tasks provider (i.e.
  // we don't if the task is already marked as completed).
  void HandleCompleteTaskButton();

  // Perform the UI update to dismiss the task view.
  void OnClearTask();

  // Called when the animation in `AnimateBubbleResize` starts.
  void OnBubbleResizeAnimationStarted();
  // Called when the animation in `AnimateBubbleResize` ends.
  void OnBubbleResizeAnimationEnded();
  // Animates resizing the bubble view after `task_item_view_` has been removed
  // from the bubble.
  void AnimateBubbleResize();
  // Updates the progression of the progress indicator.
  void UpdateProgressRing();

  // Returns whether the event is located specifically on this focus mode tray
  // view.
  bool EventTargetsTray(const ui::LocatedEvent& event) const;

  // Handles all the cleanup logic associated with closing the bubble, and may
  // reset the focus session if conditions are met. This helper function is
  // mainly used to prevent resetting the focus session when using
  // multi-displays during the ending moment.
  void CloseBubbleAndMaybeReset(bool should_reset);

  // This is used to track the current session snapshot, if any.
  std::optional<FocusModeSession::Snapshot> session_snapshot_;

  // Image view of the focus mode lamp.
  const raw_ptr<views::ImageView> image_view_;

  // The main content view of the bubble.
  raw_ptr<FocusModeCountdownView> countdown_view_ = nullptr;
  raw_ptr<FocusModeEndingMomentView> ending_moment_view_ = nullptr;

  // A box layout view which has a radio/check icon and a label for a selected
  // task.
  raw_ptr<TaskItemView> task_item_view_ = nullptr;

  // `TaskId` of the selected task shown in the `task_item_view_` if it exists.
  std::optional<TaskId> selected_task_;

  raw_ptr<views::BoxLayoutView> bubble_view_container_ = nullptr;

  // The bubble that appears after clicking the tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // An object that draws and updates the progress ring.
  std::unique_ptr<ProgressIndicator> progress_indicator_;

  // True to show the progress ring after the pulse animation of
  // `progress_indicator_`.
  bool show_progress_ring_after_animation_ = false;

  // True when the bounce in animation of the tray is done during the ending
  // moment; it will be reset to false when starting or ending a focus session,
  // or extending a focus session during the ending moment.
  bool bounce_in_animation_finished_ = false;

  // The percentage threshold that the session progress needs to exceed in order
  // to trigger a progress ring paint.
  double progress_ring_update_threshold_ = 0.0;

  base::ScopedObservation<FocusModeTasksModel, FocusModeTasksModel::Observer>
      tasks_observation_{this};

  base::WeakPtrFactory<FocusModeTray> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TRAY_H_
