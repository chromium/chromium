// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/message_center/message_center_observer.h"

namespace views {
class BoxLayoutView;
}

namespace ash {

class HoverHighlightView;
class RoundedContainer;
class Switch;
class SystemTextfield;

// This view displays the focus panel settings that a user can set.
class ASH_EXPORT FocusModeDetailedView
    : public TrayDetailedView,
      public message_center::MessageCenterObserver,
      public FocusModeController::Observer {
 public:
  METADATA_HEADER(FocusModeDetailedView);

  explicit FocusModeDetailedView(DetailedViewDelegate* delegate);
  FocusModeDetailedView(const FocusModeDetailedView&) = delete;
  FocusModeDetailedView& operator=(const FocusModeDetailedView&) = delete;
  ~FocusModeDetailedView() override;

 private:
  class TimerTextfieldController;

  friend class FocusModeDetailedViewTest;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override {}
  void AddedToWidget() override;

  // message_center::MessageCenterObserver:
  void OnQuietModeChanged(bool in_quiet_mode) override;

  // FocusModeController::Observer:
  void OnFocusModeChanged(bool in_focus_session) override;
  void OnTimerTick() override;

  // Creates the row with functionality to start and stop focus mode.
  void CreateToggleView();

  // Creates and populates the `timer_view_container_`. If we are in a focus
  // mode session, then it creates `timer_setting_view_` and adds it to the
  // `timer_view_container_`.
  void CreateTimerView();

  // Creates the row that allows for the user to adjust or set the timer
  // duration for the focus mode session.
  void CreateTimerSettingView();

  // Creates the DND rounded container.
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

  // Called whenever the session duration is adjusted.
  void OnInactiveSessionDurationChanged();

  // Sets the session duration for the focus controller and calls
  // `OnInactiveSessionDurationChanged`.
  void SetInactiveSessionDuration(base::TimeDelta duration);

  // This view contains a description of the focus session, as well as a toggle
  // button for staring/ending focus mode.
  raw_ptr<HoverHighlightView> toggle_view_ = nullptr;
  // Container that holds a header and the `timer_setting_view_`.
  raw_ptr<RoundedContainer> timer_view_container_ = nullptr;
  // This view contains the timer view for the user to adjust the focus session
  // duration when we are not in a focus session.
  raw_ptr<views::BoxLayoutView> timer_setting_view_ = nullptr;
  // Textfield that the user can use to set the timer duration.
  raw_ptr<SystemTextfield> timer_textfield_ = nullptr;
  // Handles input validation and events for the `timer_textfield`.
  std::unique_ptr<TimerTextfieldController> timer_textfield_controller_;
  // This view contains controls for selecting the focus scene (background +
  // audio), as well as volume controls.
  raw_ptr<RoundedContainer> scene_view_ = nullptr;

  // This view contains a toggle for turning on/off DND.
  raw_ptr<RoundedContainer> do_not_disturb_view_ = nullptr;
  raw_ptr<Switch> do_not_disturb_toggle_button_ = nullptr;

  // Updates the subheading of the toggle view so that it can correctly show
  // what time the focus mode session will end. This is activated when the panel
  // is open but focus mode is not active, because we still need to update the
  // subheading to say what time the focus mode session would end. In order to
  // track this, this timer fires when the clock minute changes.
  base::OneShotTimer clock_timer_;

  base::WeakPtrFactory<FocusModeDetailedView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_H_
