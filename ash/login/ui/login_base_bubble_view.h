// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_BASE_BUBBLE_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_BASE_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

namespace ash {

// Base bubble view for login screen bubbles.
class ASH_EXPORT LoginBaseBubbleView : public views::BubbleDialogDelegateView {
 public:
  explicit LoginBaseBubbleView(views::View* anchor_view);
  ~LoginBaseBubbleView() override;

  // views::BubbleDialogDelegateView:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
  int GetDialogButtons() const override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginBaseBubbleView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_BASE_BUBBLE_VIEW_H_
