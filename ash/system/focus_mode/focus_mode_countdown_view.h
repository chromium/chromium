// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_COUNTDOWN_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_COUNTDOWN_VIEW_H_

#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class Label;
class ProgressBar;
}  // namespace views

namespace ash {

// The bubble associated with the `FocusModeTray`. Contains a timer with the
// amount of time left in the focus session, buttons to end the focus session
// and add 10 minutes to the focus session, and a progress bar showing the
// total focus session time and how much of the focus session has already
// elapsed.
class ASH_EXPORT FocusModeCountdownView : public views::FlexLayoutView,
                                          public FocusModeController::Observer {
 public:
  FocusModeCountdownView();
  FocusModeCountdownView(const FocusModeCountdownView&) = delete;
  FocusModeCountdownView& operator=(const FocusModeCountdownView&) = delete;
  ~FocusModeCountdownView() override;

  // FocusModeController::Observer:
  void OnFocusModeChanged(bool in_focus_session) override {}
  void OnTimerTick() override;

 private:
  void UpdateUI();

  // The main timer label, displays the amount of time left in the focus
  // session.
  raw_ptr<views::Label, ExperimentalAsh> time_remaining_label_ = nullptr;

  // The timer on the left of the bar, displays the amount of time that has
  // already passed during the focus session.
  raw_ptr<views::Label, ExperimentalAsh> time_elapsed_label_ = nullptr;

  // The timer on the right of the bar, displays the total session duration.
  raw_ptr<views::Label, ExperimentalAsh> time_total_label_ = nullptr;

  // The timer progress bar.
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_COUNTDOWN_VIEW_H_
