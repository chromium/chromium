// Copyright 2017 The Chromium Authors
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

  SessionAbortedDialog(const SessionAbortedDialog&) = delete;
  SessionAbortedDialog& operator=(const SessionAbortedDialog&) = delete;

  // views::View overrides.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  SessionAbortedDialog();
  ~SessionAbortedDialog() override;

  void InitDialog(const std::string& user_email);
};

}  // namespace ash

#endif  // ASH_SESSION_SESSION_ABORTED_DIALOG_H_
