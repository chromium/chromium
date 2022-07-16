// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_

#include "ui/views/controls/button/image_button.h"

namespace ash {

// The button with `kUnifiedMenuExpandIcon`. This button can be set as expanded
// or collapsed through SetExpandedAmount and the icon will be rotated on the
// `expanded_amount_`. Expanded is the default state.
class CollapseButton : public views::ImageButton {
 public:
  explicit CollapseButton(PressedCallback callback);

  CollapseButton(const CollapseButton&) = delete;
  CollapseButton& operator=(const CollapseButton&) = delete;

  ~CollapseButton() override;

  // Change the expanded state. The icon will change.
  void SetExpandedAmount(double expanded_amount);

  // views::ImageButton:
  gfx::Size CalculatePreferredSize() const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

 private:
  double expanded_amount_ = 1.0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_
