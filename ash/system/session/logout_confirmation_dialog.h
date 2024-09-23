// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SESSION_LOGOUT_CONFIRMATION_DIALOG_H_
#define ASH_SYSTEM_SESSION_LOGOUT_CONFIRMATION_DIALOG_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
}

namespace ash {

class LogoutConfirmationController;

// A dialog that asks the user to confirm or deny logout. The dialog shows a
// countdown and informs the user that a logout will happen automatically if no
// choice is made before the countdown has expired.
class LogoutConfirmationDialog : public views::DialogDelegateView {
  METADATA_HEADER(LogoutConfirmationDialog, views::DialogDelegateView)
 public:
  LogoutConfirmationDialog(LogoutConfirmationController* controller,
                           base::TimeTicks logout_time);

  LogoutConfirmationDialog(const LogoutConfirmationDialog&) = delete;
  LogoutConfirmationDialog& operator=(const LogoutConfirmationDialog&) = delete;

  ~LogoutConfirmationDialog() override;

  void Update(base::TimeTicks logout_time);

  // Called when |controller_| is no longer valid.
  void ControllerGone();

  // views::WidgetDelegate:
  void WindowClosing() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  void UpdateLabel();
  void OnDialogAccepted();

  raw_ptr<LogoutConfirmationController> controller_;
  base::TimeTicks logout_time_;

  raw_ptr<views::Label> label_;

  base::RepeatingTimer update_timer_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_SESSION_LOGOUT_CONFIRMATION_DIALOG_H_
