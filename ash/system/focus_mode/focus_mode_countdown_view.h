// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_COUNTDOWN_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_COUNTDOWN_VIEW_H_

#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class Label;
class ProgressBar;
}  // namespace views

namespace ash {

class PillButton;

// Contains a timer with the amount of time left in the focus session, buttons
// to end the focus session and add 10 minutes to the focus session, and a
// progress bar showing the total focus session time and how much of the focus
// session has already elapsed. The button to end the focus session is only
// included if `include_end_button`, otherwise just the button to add 10 minutes
// is included. This view's parent needs to call `UpdateUI()` to first populate
// the UI before it is shown for the first time, and on timer tick.
class ASH_EXPORT FocusModeCountdownView : public views::FlexLayoutView {
  METADATA_HEADER(FocusModeCountdownView, views::FlexLayoutView)

 public:
  explicit FocusModeCountdownView(bool include_end_button);
  FocusModeCountdownView(const FocusModeCountdownView&) = delete;
  FocusModeCountdownView& operator=(const FocusModeCountdownView&) = delete;
  ~FocusModeCountdownView() override = default;

  // Updates the timers and progress bar. This must be called from this view's
  // parent's `OnTimerTick()` and when the view is first created.
  void UpdateUI(const FocusModeSession::Snapshot& session_snapshot);

 private:
  friend class FocusModeCountdownViewTest;
  friend class FocusModeTrayTest;

  // The main timer label, displays the amount of time left in the focus
  // session.
  raw_ptr<views::Label> time_remaining_label_ = nullptr;

  // The timer on the left of the bar, displays the amount of time that has
  // already passed during the focus session.
  raw_ptr<views::Label> time_elapsed_label_ = nullptr;

  // The timer on the right of the bar, displays the total session duration.
  raw_ptr<views::Label> time_total_label_ = nullptr;

  // The timer progress bar.
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;

  // The `+10 min` button.
  raw_ptr<PillButton> extend_session_duration_button_ = nullptr;

  // Whether to create the "End" button to end the focus session.
  const bool include_end_button_;
  // The `End` button.
  raw_ptr<PillButton> end_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_COUNTDOWN_VIEW_H_
