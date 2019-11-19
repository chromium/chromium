// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_SESSION_ABORTED_DIALOG_H_
#define ASH_SESSION_SESSION_ABORTED_DIALOG_H_

#include <string>

#include "ui/views/window/dialog_delegate.h"

namespace ash {

// Dialog for an aborted multi-profile session due to a user policy change.
// The dialog gives the user an explanation but no option to avoid shutting down
// the session.
class SessionAbortedDialog : public views::DialogDelegateView {
 public:
  static void Show(const std::string& user_email);

  // views::DialogDelegate overrides.
  bool Accept() override;
  int GetDialogButtons() const override;

  // views::WidgetDelegate overrides.
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;

  // views::View overrides.
  gfx::Size CalculatePreferredSize() const override;

 private:
  SessionAbortedDialog();
  ~SessionAbortedDialog() override;

  void InitDialog(const std::string& user_email);

  DISALLOW_COPY_AND_ASSIGN(SessionAbortedDialog);
};

}  // namespace ash

#endif  // ASH_SESSION_SESSION_ABORTED_DIALOG_H_
