// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SESSION_LOGOUT_CONFIRMATION_DIALOG_H_
#define ASH_SYSTEM_SESSION_LOGOUT_CONFIRMATION_DIALOG_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
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
 public:
  LogoutConfirmationDialog(LogoutConfirmationController* controller,
                           base::TimeTicks logout_time);
  ~LogoutConfirmationDialog() override;

  void Update(base::TimeTicks logout_time);

  // Called when |controller_| is no longer valid.
  void ControllerGone();

  // views::DialogDelegateView:
  bool Accept() override;

  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;

 private:
  void UpdateLabel();

  LogoutConfirmationController* controller_;
  base::TimeTicks logout_time_;

  views::Label* label_;

  base::RepeatingTimer update_timer_;

  DISALLOW_COPY_AND_ASSIGN(LogoutConfirmationDialog);
};

}  // namespace ash

#endif  // ASH_SYSTEM_SESSION_LOGOUT_CONFIRMATION_DIALOG_H_
