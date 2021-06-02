// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PUBLIC_ACCOUNT_WARNING_DIALOG_H_
#define ASH_LOGIN_UI_PUBLIC_ACCOUNT_WARNING_DIALOG_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

class LoginExpandedPublicAccountView;

// Dialog for displaying public session warning. This is shown when a user
// clicks on the learn more link on the pubic account expanded view.
class ASH_EXPORT PublicAccountWarningDialog : public views::DialogDelegateView {
 public:
  PublicAccountWarningDialog(
      base::WeakPtr<LoginExpandedPublicAccountView> controller);
  ~PublicAccountWarningDialog() override;

  bool IsVisible();
  void Show();

  // views::DialogDelegate:
  void AddedToWidget() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

 private:
  base::WeakPtr<LoginExpandedPublicAccountView> controller_;

  DISALLOW_COPY_AND_ASSIGN(PublicAccountWarningDialog);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PUBLIC_ACCOUNT_WARNING_DIALOG_H_
