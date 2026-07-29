// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_bubble.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/functional/bind.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/bubble/bubble_frame_view.h"

namespace {

views::BubbleBorder::Arrow GetArrow(ash::ShelfAlignment alignment) {
  switch (alignment) {
    case ash::ShelfAlignment::kBottom:
    case ash::ShelfAlignment::kBottomLocked:
      return views::BubbleBorder::BOTTOM_CENTER;
    case ash::ShelfAlignment::kLeft:
      return views::BubbleBorder::LEFT_CENTER;
    case ash::ShelfAlignment::kRight:
      return views::BubbleBorder::RIGHT_CENTER;
  }
  return views::BubbleBorder::Arrow::NONE;
}

}  // namespace

namespace ash {

ShelfBubble::ShelfBubble(
    views::View* anchor,
    ShelfAlignment alignment,
    bool for_tooltip,
    std::optional<views::BubbleBorder::Arrow> arrow_position)
    : views::BubbleDialogDelegateView(
          anchor,
          arrow_position.value_or(GetArrow(alignment))),
      for_tooltip_(for_tooltip) {
  SetBackgroundColor(SK_ColorTRANSPARENT);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetUseAnchorWindowBounds(false);
  set_frame_margins({.title = gfx::Insets()});

  if (for_tooltip_) {
    set_available_screen_bounds_callback(
        base::BindRepeating([](const gfx::Rect& rect) {
          return display::Screen::Get()
              ->GetDisplayNearestPoint(rect.CenterPoint())
              .bounds();
        }));
  }

  // Place the bubble in the same display as the anchor.
  set_parent_window(
      anchor_widget()->GetNativeWindow()->GetRootWindow()->GetChildById(
          kShellWindowId_SettingBubbleContainer));

  // We override the role because the base class sets it to alert dialog,
  // which results in each tooltip title being announced twice on screen
  // readers each time it is shown.
  SetAccessibleWindowRole(ax::mojom::Role::kDialog);
}

ShelfBubble::~ShelfBubble() = default;

void ShelfBubble::CreateBubble() {
  // Actually create the bubble.
  views::BubbleDialogDelegateView::CreateBubble(this);

  // Settings that should only be changed just after bubble creation.
  GetBubbleFrameView()->SetRoundedCorners(gfx::RoundedCornersF(border_radius_));
  GetBubbleFrameView()->SetBackgroundColor(background_color());
}

BEGIN_METADATA(ShelfBubble)
END_METADATA

}  // namespace ash
