// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/arrow_button_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {
namespace {

// Arrow icon size.
constexpr int kArrowIconSizeDp = 20;
// An alpha value for disabled button.
constexpr SkAlpha kButtonDisabledAlpha = 0x80;

}  // namespace

ArrowButtonView::ArrowButtonView(views::ButtonListener* listener, int size)
    : LoginButton(listener), size_(size) {
  SetPreferredSize(gfx::Size(size, size));
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // Layer rendering is needed for animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetImage(Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kLockScreenArrowIcon, kArrowIconSizeDp,
                                 SK_ColorWHITE));
  SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(kLockScreenArrowIcon, kArrowIconSizeDp,
                            SkColorSetA(SK_ColorWHITE, kButtonDisabledAlpha)));
}

ArrowButtonView::~ArrowButtonView() = default;

void ArrowButtonView::PaintButtonContents(gfx::Canvas* canvas) {
  const gfx::Rect rect(GetContentsBounds());

  // Draw background.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(background_color_);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), size_ / 2, flags);

  // Draw arrow icon.
  views::ImageButton::PaintButtonContents(canvas);
}
void ArrowButtonView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  LoginButton::GetAccessibleNodeData(node_data);
  // TODO(tbarzic): Fix this - https://crbug.com/961930.
  if (GetAccessibleName().empty())
    node_data->SetNameExplicitlyEmpty();
}

void ArrowButtonView::SetBackgroundColor(SkColor color) {
  background_color_ = color;
  SchedulePaint();
}

}  // namespace ash
