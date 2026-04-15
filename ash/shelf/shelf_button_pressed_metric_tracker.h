// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_BUTTON_PRESSED_METRIC_TRACKER_H_
#define ASH_SHELF_SHELF_BUTTON_PRESSED_METRIC_TRACKER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
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
//
class ASH_EXPORT ShelfButtonPressedMetricTracker {
 public:
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
  // Records UMA metrics for the input source when a button is pressed.
  void RecordButtonPressedSource(const ui::Event& event);

  // Records UMA metrics for the action performed when a button is pressed.
  void RecordButtonPressedAction(ShelfAction performed_action);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_BUTTON_PRESSED_METRIC_TRACKER_H_
