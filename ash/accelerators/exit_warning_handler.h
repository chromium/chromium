// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_EXIT_WARNING_HANDLER_H_
#define ASH_ACCELERATORS_EXIT_WARNING_HANDLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/timer/timer.h"
#include "ui/base/accelerators/accelerator.h"

namespace views {
class Widget;
}

namespace ash {

// In order to avoid accidental exits when the user presses the exit shortcut by
// mistake, we require that the user press it twice within a short period of
// time. During that time we show a popup informing the user of this.
//
// Notes:
//
// The corresponding accelerator must not be repeatable (see kRepeatableActions
// in accelerator_table.cc). Otherwise, the "Double Press Exit" will be
// activated just by holding it down, i.e. probably every time.
//
// State Transition Diagrams:
//
//  IDLE
//   | Press
//  WAIT_FOR_DOUBLE_PRESS action: show ui & start timers
//   | Press (before time limit )
//  EXITING action: hide ui, stop timer, exit
//
//  IDLE
//   | Press
//  WAIT_FOR_DOUBLE_PRESS action: show ui & start timers
//   | T timer expires
//  IDLE action: hide ui
//

class AcceleratorControllerTest;

class ASH_EXPORT ExitWarningHandler {
 public:
  ExitWarningHandler();
  ExitWarningHandler(const ExitWarningHandler&) = delete;
  ExitWarningHandler& operator=(const ExitWarningHandler&) = delete;
  ~ExitWarningHandler();

  // Handles accelerator for exit (Ctrl-Shift-Q).
  void HandleAccelerator();

 private:
  friend class AcceleratorControllerTest;
  friend class OverviewSessionTest;

  enum State { IDLE, WAIT_FOR_DOUBLE_PRESS, EXITING };

  // Performs actions when the time limit is exceeded.
  void TimerAction();

  void StartTimer();
  void CancelTimer();

  void Show();
  void Hide();

  State state_;
  std::unique_ptr<views::Widget> widget_;
  base::OneShotTimer timer_;

  // Flag to suppress starting the timer for testing. For test we call
  // TimerAction() directly to simulate the expiration of the timer.
  bool stub_timer_for_test_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_EXIT_WARNING_HANDLER_H_
