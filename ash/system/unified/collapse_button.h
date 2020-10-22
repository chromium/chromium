// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_

#include "ash/system/unified/custom_shape_button.h"
#include "base/bind.h"

namespace ash {

// Collapse button shown in TopShortcutsView with TopShortcutButtons.
// UnifiedSystemTrayBubble will support collapsed state where the height of the
// bubble is smaller, and some rows and labels will be omitted.
// By pressing the button, the state of the bubble will be toggled.
class CollapseButton : public CustomShapeButton {
 public:
  explicit CollapseButton(views::ButtonListener* listener);
  ~CollapseButton() override;

  // Change the expanded state. The icon will change.
  void SetExpandedAmount(double expanded_amount);

  // CustomShapeButton:
  gfx::Size CalculatePreferredSize() const override;
  SkPath CreateCustomShapePath(const gfx::Rect& bounds) const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

 private:
  // Update the vector icon on initializing CollapseButton or when the |Enabled|
  // property of CollapseButton changes. The vector icon will have different
  // colors on the |Enabled| property.
  void UpdateVectorIcon();

  double expanded_amount_ = 1.0;
  views::PropertyChangedSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&CollapseButton::UpdateVectorIcon,
                              base::Unretained(this)));

  DISALLOW_COPY_AND_ASSIGN(CollapseButton);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_
