// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_BUTTON_H_
#define ASH_LOGIN_UI_LOGIN_BUTTON_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace ash {

// This class adds ripple effects for touch targets in the lockscreen.
class ASH_EXPORT LoginButton : public views::ImageButton {
 public:
  explicit LoginButton(PressedCallback callback);
  ~LoginButton() override;

  // views::InkDropHost:
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

 protected:
  virtual int GetInkDropRadius() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginButton);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_BUTTON_H_
