// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_task_view.h"
#include "ash/system/model/clock_observer.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class BoxLayoutView;
class Label;
}  // namespace views

namespace ash {

namespace {
class PanelRowAnimator;
}

class FocusModeCountdownView;
class FocusModeSoundsView;
class FocusModeTaskView;
class HoverHighlightView;
class IconButton;
class RoundedContainer;
class Switch;
class SystemTextfield;

// This view displays the focus panel settings that a user can set.
class ASH_EXPORT FocusModeDetailedView : public TrayDetailedView,
                                         public FocusModeController::Observer,
                                         public ClockObserver {
  METADATA_HEADER(FocusModeDetailedView, TrayDetailedView)

 public:
  // Ids to easily find child views in `FocusModeDetailedView`. Unique only
  // within the `FocusModeDetailedView`.
  enum ViewId {
    kTimerView = 1000,
    kTaskView,
    kSoundView,
    kTimerTextfield,
    kToggleFocusButton
  };

  explicit FocusModeDetailedView(DetailedViewDelegate* delegate);
  FocusModeDetailedView(const FocusModeDetailedView&) = delete;
  FocusModeDetailedView& operator=(const FocusModeDetailedView&) = delete;
  ~FocusModeDetailedView() override;

  // ClockObserver:
  void OnDateFormatChanged() override;
  void OnSystemClockTimeUpdated() override;
  void OnSystemClockCanSetTimeChanged(bool can_set_time) override;
  void Refresh() override;

 private:
  class TimerTextfieldController;

  friend class FocusModeDetailedViewTest;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override {}
  void AddedToWidget() override;

  // FocusModeController::Observer:
  void OnFocusModeChanged(FocusModeSession::State session_state) override;
  void OnTimerTick(const FocusModeSession::Snapshot& session_snapshot) override;
  void OnActiveSessionDurationChanged(
      const FocusModeSession::Snapshot& session_snapshot) override;

  // Creates the row with functionality to start and stop focus mode.
  void CreateToggleView();

  // Updates the accessibility text of the toggle button based on whether focus
  // is in session and the current session duration.
  void UpdateToggleButtonAccessibility(bool in_focus_session);

  // Updates the accessibility text of the timer adjustment buttons.
  void UpdateTimerAdjustmentButtonAccessibility();

  // Creates the row with the timer and functionality to add time to the focus
  // session.
  void CreateTimerView();

  // Updates the row with the timer and functionality to add time to the focus
  // session based on whether focus is in session.
  void UpdateTimerView(bool in_focus_session);

  // Clear the focus for `timer_textfield_` when it's be inactive and update the
  // session duration.
  void HandleTextfieldActivationChange();

  // Creates the row with the task elements. Creates the textfield to allow a
  // user to manually input a task and creates a chip carousel list of tasks to
  // allow the user to select a task. Once the user enters a task in the
  // textfield or selects a task from the list, this view only shows the
  // selected saved task item view and the header.
  void CreateTaskView(bool is_network_connected);

  // Creates the container `focus_mode_sounds_view_`.
  void CreateSoundsView(
      const base::flat_set<focus_mode_util::SoundType>& sound_sections,
      bool is_network_connected);

  // Creates the DND rounded container. This view will be visible only when
  // there is no active focus session. The toggle button in this view will
  // represent if we should toggle on the system DND state for a new focus
  // session.
  void CreateDoNotDisturbContainer();

  // Handles clicks on the do not disturb toggle button.
  void OnDoNotDisturbToggleClicked();

  // Called whenever `clock_timer_` finishes running to update the subheading
  // and reset the clock timer for the next minute.
  void OnClockMinutePassed();

  // Starts `clock_timer_`.
  void StartClockTimer();

  // Increments or decrements the session duration by one step.
  // This is only used outside of a focus session.
  void AdjustInactiveSessionDuration(bool decrement);

  // Called whenever the session duration is adjusted. Updates the labels and
  // button visibilities in the timer setting view.
  void UpdateTimerSettingViewUI();

  // Sets the session duration for the focus controller and calls
  // `UpdateTimerSettingViewUI`.
  void SetInactiveSessionDuration(base::TimeDelta duration);

  // Updates the `toggle_view_` sub text if a focus session is active and
  // `end_time_label_` if it isn't.
  void UpdateEndTimeLabel();

  // This view contains a description of the focus session, as well as a toggle
  // button for staring/ending focus mode.
  raw_ptr<HoverHighlightView> toggle_view_ = nullptr;
  // This view contains the timer view for the user to adjust the focus session
  // duration.
  raw_ptr<RoundedContainer> timer_view_container_ = nullptr;
  // The countdown view inside `timer_view_container_` when focus is in session.
  raw_ptr<FocusModeCountdownView> timer_countdown_view_ = nullptr;
  // This view contains the timer view for the user to adjust the focus session
  // duration when we are not in a focus session.
  raw_ptr<views::BoxLayoutView> timer_setting_view_ = nullptr;
  // Textfield that the user can use to set the timer duration.
  raw_ptr<SystemTextfield> timer_textfield_ = nullptr;
  // Handles input validation and events for the `timer_textfield`.
  std::unique_ptr<TimerTextfieldController> timer_textfield_controller_;
  // The decrement and increment buttons in the `timer_setting_view_`.
  raw_ptr<IconButton> timer_decrement_button_ = nullptr;
  raw_ptr<IconButton> timer_increment_button_ = nullptr;
  // The visual "minutes" label that pairs with the `timer_textfield_` timer
  // duration.
  raw_ptr<views::Label> minutes_label_ = nullptr;
  // A label that displays the end time of the focus session when focus is
  // not active.
  raw_ptr<views::Label> end_time_label_ = nullptr;

  // The view contains a header view and a `focus_mode_task_view_`.
  raw_ptr<RoundedContainer> task_view_container_ = nullptr;
  std::unique_ptr<PanelRowAnimator> task_view_animator_;
  raw_ptr<FocusModeTaskView> focus_mode_task_view_ = nullptr;
  raw_ptr<FocusModeSoundsView> focus_mode_sounds_view_ = nullptr;
  std::unique_ptr<PanelRowAnimator> sounds_view_animator_;

  // This view contains a toggle for turning on/off DND.
  raw_ptr<RoundedContainer> do_not_disturb_view_ = nullptr;
  raw_ptr<Switch> do_not_disturb_toggle_button_ = nullptr;

  // Updates `end_time_label_` so that it can correctly show what time the focus
  // mode session will end. This is activated when the panel is open but focus
  // mode is not active, because we still need to update the label to say what
  // time the focus mode session would end. In order to track this, this timer
  // fires when the clock minute changes.
  base::OneShotTimer clock_timer_;

  base::WeakPtrFactory<FocusModeDetailedView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_H_
