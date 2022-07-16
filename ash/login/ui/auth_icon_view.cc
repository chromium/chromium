// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/auth_icon_view.h"

#include "ash/style/ash_color_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {
constexpr int kAuthIconSizeDp = 32;
}

AuthIconView::AuthIconView()
    : AnimatedRoundedImageView(gfx::Size(kAuthIconSizeDp, kAuthIconSizeDp),
                               /*corner_radius=*/0) {}

AuthIconView::~AuthIconView() = default;

void AuthIconView::SetIcon(const gfx::VectorIcon& icon) {
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);

  SetImage(gfx::CreateVectorIcon(icon, kAuthIconSizeDp, icon_color));
}

// views::View:
void AuthIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP &&
      event->type() != ui::ET_GESTURE_TAP_DOWN)
    return;

  if (on_tap_or_click_callback_) {
    on_tap_or_click_callback_.Run();
  }
}

// views::View:
bool AuthIconView::OnMousePressed(const ui::MouseEvent& event) {
  if (on_tap_or_click_callback_) {
    on_tap_or_click_callback_.Run();
    return true;
  }
  return false;
}

}  // namespace ash
