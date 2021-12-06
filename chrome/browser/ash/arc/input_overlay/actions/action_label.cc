// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/action_label.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"

namespace arc {
namespace input_overlay {

constexpr int kWidthPadding = 10;
constexpr int kMinimumLabelWidth = 20;

ActionLabel::ActionLabel() : views::Label() {}
ActionLabel::ActionLabel(const std::u16string& text) : views::Label(text) {
  SetDefaultViewMode();
}

ActionLabel::~ActionLabel() = default;

void ActionLabel::SetDefaultViewMode() {
  // TODO(cuicuiruan): Replace it with required color once UI/UX specs are
  // confirmed.
  SetBackground(views::CreateSolidBackground(gfx::kGoogleGrey400));
}

void ActionLabel::SetPositionFromCenterPosition(gfx::PointF& center_position) {
  auto size = GetPreferredSize();
  SetSize(size);
  int left = std::max(0, (int)(center_position.x() - size.width() / 2));
  int top = std::max(0, (int)(center_position.y() - size.height() / 2));
  // SetPosition function needs the top-left position.
  SetPosition(gfx::Point(left, top));
}

gfx::Size ActionLabel::CalculatePreferredSize() const {
  auto size = Label::CalculatePreferredSize();
  size.set_width(size.width() + kWidthPadding);
  if (size.width() < kMinimumLabelWidth)
    size.set_width(kMinimumLabelWidth);
  return size;
}

}  // namespace input_overlay
}  // namespace arc
