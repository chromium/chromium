// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action_view.h"

#include "chrome/browser/ash/arc/input_overlay/actions/action_label.h"

namespace arc {
namespace input_overlay {

ActionView::ActionView(Action* action) : views::View(), action_(action) {}
ActionView::~ActionView() = default;

void ActionView::SetDisplayMode(DisplayMode mode) {
  if ((!editable_ && mode == DisplayMode::kEdit) || mode == DisplayMode::kMenu)
    return;
  if (circle_)
    circle_->SetDisplayMode(mode);
  for (auto* label : labels_)
    label->SetDisplayMode(mode);
}

void ActionView::SetPositionFromCenterPosition(gfx::PointF& center_position) {
  int left = std::max(0, (int)(center_position.x() - center_.x()));
  int top = std::max(0, (int)(center_position.y() - center_.y()));
  // SetPosition function needs the top-left position.
  SetPosition(gfx::Point(left, top));
}

}  // namespace input_overlay
}  // namespace arc
