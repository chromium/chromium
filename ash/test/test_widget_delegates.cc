// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_widget_delegates.h"

#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace ash {

CenteredBubbleDialogModelHost::CenteredBubbleDialogModelHost(
    views::Widget* anchor_widget,
    const gfx::Size& size,
    bool close_on_deactivate)
    : views::BubbleDialogModelHost(ui::DialogModel::Builder().Build(),
                                   /*anchor=*/nullptr,
                                   views::BubbleBorder::Arrow::NONE),
      size_(size) {
  set_parent_window(anchor_widget->GetNativeWindow());
  SetAnchorWidget(anchor_widget);
  set_close_on_deactivate(close_on_deactivate);
  set_desired_bounds_delegate(
      base::BindRepeating(&CenteredBubbleDialogModelHost::GetDesiredBounds,
                          base::Unretained(this)));
}

gfx::Rect CenteredBubbleDialogModelHost::GetDesiredBounds() const {
  if (!anchor_widget()) {
    // Anchor widget may be deleted first.
    return gfx::Rect(size_);
  }
  CHECK(anchor_widget());
  auto centered_bounds = anchor_widget()->GetWindowBoundsInScreen();
  centered_bounds.ToCenteredSize(size_);
  return centered_bounds;
}

}  // namespace ash
