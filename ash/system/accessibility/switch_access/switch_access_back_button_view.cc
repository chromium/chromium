// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access/switch_access_back_button_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/accessibility/floating_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "base/functional/bind.h"
#include "cc/paint/paint_flags.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/mojom/ax_node_data.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {
// The width of a single color focus ring, in density-independent pixels.
constexpr int kFocusRingSingleColorWidthDp = 2;
// Additional buffer needed to prevent clipping at the focus ring's edges.
constexpr int kFocusRingBufferDp = 1;

constexpr int kRadiusDp = 18;
}  // namespace

SwitchAccessBackButtonView::SwitchAccessBackButtonView(bool for_menu) {
  // Calculate the side length of the bounding box, with room for the two-color
  // focus ring on either side.
  int side_length = 2 * (kRadiusDp + GetFocusRingWidthPerSide());

  views::Builder<SwitchAccessBackButtonView>(this)
      .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
      .AddChildren(
          views::Builder<FloatingMenuButton>()
              .CopyAddressTo(&back_button_)
              .SetVectorIcon(for_menu ? kSwitchAccessCloseIcon
                                      : kSwitchAccessBackIcon)
              .SetTooltipText(l10n_util::GetStringUTF16(
                  IDS_ASH_SWITCH_ACCESS_BACK_BUTTON_DESCRIPTION))
              .SetPreferredSize(gfx::Size(2 * kRadiusDp, 2 * kRadiusDp))
              .SetDrawHighlight(true)
              .SetCallback(base::BindRepeating(
                  &SwitchAccessBackButtonView::OnButtonPressed,
                  base::Unretained(this))))
      .SetSize(gfx::Size(side_length, side_length))
      .BuildChildren();

  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
}

int SwitchAccessBackButtonView::GetFocusRingWidthPerSide() {
  return 2 * kFocusRingSingleColorWidthDp + kFocusRingBufferDp;
}

void SwitchAccessBackButtonView::SetFocusRing(bool should_show) {
  if (show_focus_ring_ == should_show) {
    return;
  }
  show_focus_ring_ = should_show;
  SchedulePaint();
}

void SwitchAccessBackButtonView::SetForMenu(bool for_menu) {
  if (for_menu) {
    back_button_->SetVectorIcon(kSwitchAccessCloseIcon);
  } else {
    back_button_->SetVectorIcon(kSwitchAccessBackIcon);
  }
}

gfx::Size SwitchAccessBackButtonView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size preferred_size =
      views::BoxLayoutView::CalculatePreferredSize(available_size);
  if (available_size.width().is_bounded()) {
    preferred_size.set_height(available_size.width().value());
  }
  return preferred_size;
}

void SwitchAccessBackButtonView::OnPaint(gfx::Canvas* canvas) {
  auto* color_provider = GetColorProvider();
  gfx::Rect rect(GetContentsBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color_provider->GetColor(kColorAshShieldAndBase80));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), kRadiusDp, flags);

  if (!show_focus_ring_) {
    return;
  }

  flags.setColor(
      color_provider->GetColor(kColorAshSwitchAccessInnerStrokeColor));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kFocusRingSingleColorWidthDp);
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()),
                     kRadiusDp + kFocusRingSingleColorWidthDp, flags);

  flags.setColor(
      color_provider->GetColor(kColorAshSwitchAccessOuterStrokeColor));
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()),
                     kRadiusDp + (2 * kFocusRingSingleColorWidthDp), flags);
}

void SwitchAccessBackButtonView::OnButtonPressed() {
  NotifyAccessibilityEvent(ax::mojom::Event::kClicked,
                           /*send_native_event=*/false);
}

BEGIN_METADATA(SwitchAccessBackButtonView)
END_METADATA

}  // namespace ash
