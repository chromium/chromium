// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_SCROLL_ARROW_BUTTON_H_
#define ASH_WM_DESKS_SCROLL_ARROW_BUTTON_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class DesksBarView;

// The scroll button used by scrollable desks bar in Bento. An arrow icon will
// be added to the button. But Button used here instead of ImageButton since we
// want to paint the button on arrow type and RTL layout differently, also
// customize the icon's layout.
class ASH_EXPORT ScrollArrowButton : public views::Button {
 public:
  ScrollArrowButton(PressedCallback callback,
                    bool is_left_arrow,
                    DesksBarView* bar_view);
  ScrollArrowButton(const ScrollArrowButton&) = delete;
  ScrollArrowButton& operator=(const ScrollArrowButton&) = delete;
  ~ScrollArrowButton() override = default;

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;
  const char* GetClassName() const override;

 private:
  const bool is_left_arrow_;
  DesksBarView* const bar_view_;  // Not owned.
};

}  // namespace ash

#endif  // ASH_WM_DESKS_SCROLL_ARROW_BUTTON_H_
