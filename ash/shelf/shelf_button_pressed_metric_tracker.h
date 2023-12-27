// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_BUTTON_PRESSED_METRIC_TRACKER_H_
#define ASH_SHELF_SHELF_BUTTON_PRESSED_METRIC_TRACKER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "ui/events/event.h"

namespace views {
class Button;
}  // namespace views

namespace ash {

// Tracks UMA metrics based on shelf button press actions. More specifically
// data is added to the following user actions and histograms:
//
//    User Actions:
//      - Launcher_ButtonPressed_Mouse
//      - Launcher_ButtonPressed_Touch
//      - Launcher_LaunchTask
//      - Launcher_MinimizeTask
//      - Launcher_SwitchTask
//    Histograms:
//      - Ash.Shelf.TimeBetweenWindowMinimizedAndActivatedActions
//
class ASH_EXPORT ShelfButtonPressedMetricTracker {
 public:
  static const char
      kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName[];

  ShelfButtonPressedMetricTracker();

  ShelfButtonPressedMetricTracker(const ShelfButtonPressedMetricTracker&) =
      delete;
  ShelfButtonPressedMetricTracker& operator=(
      const ShelfButtonPressedMetricTracker&) = delete;

  ~ShelfButtonPressedMetricTracker();

  // Records metrics based on the |event|, |sender|, and |performed_action|.
  void ButtonPressed(const ui::Event& event,
                     const views::Button* sender,
                     ShelfAction performed_action);

 private:
  friend class ShelfButtonPressedMetricTrackerTestAPI;

  // Records UMA metrics for the input source when a button is pressed.
  void RecordButtonPressedSource(const ui::Event& event);

  // Records UMA metrics for the action performed when a button is pressed.
  void RecordButtonPressedAction(ShelfAction performed_action);

  // Records UMA metrics for the elapsed time since the last window minimize
  // action.
  void RecordTimeBetweenMinimizedAndActivated();

  // Returns true if a window activation action triggered by |sender| would
  // be subsequent to the last minimize window action.
  bool IsSubsequentActivationEvent(const views::Button* sender) const;

  // Caches state data for a window minimized action. The |sender| is the button
  // that caused the action.
  void SetMinimizedData(const views::Button* sender);

  // Resets the state data associated with the last window minimize action.
  void ResetMinimizedData();

  // Time source for performed action times.
  raw_ptr<const base::TickClock> tick_clock_;

  // Stores the time of the last window minimize action.
  base::TimeTicks time_of_last_minimize_;

  // Stores the source button of the last window minimize action.
  // NOTE: This may become stale and should not be operated on. Not owned.
  raw_ptr<const views::Button, DanglingUntriaged>
      last_minimized_source_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_BUTTON_PRESSED_METRIC_TRACKER_H_
