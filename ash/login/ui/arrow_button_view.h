// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_ARROW_BUTTON_VIEW_H_
#define ASH_LOGIN_UI_ARROW_BUTTON_VIEW_H_

#include "ash/login/ui/login_button.h"
#include "ui/views/controls/image_view.h"

namespace ash {

// A round button with arrow icon in the middle.
// This will be used by LoginPublicAccountUserView and expanded public account
// view.
class ArrowButtonView : public LoginButton {
 public:
  ArrowButtonView(views::ButtonListener* listener, int size);
  ~ArrowButtonView() override;

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Set background color of the button.
  void SetBackgroundColor(SkColor color);

 private:
  int size_;
  SkColor background_color_;

  DISALLOW_COPY_AND_ASSIGN(ArrowButtonView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_ARROW_BUTTON_VIEW_H_
