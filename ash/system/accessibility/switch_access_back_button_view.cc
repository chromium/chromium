// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access_back_button_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/floating_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "cc/paint/paint_flags.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/mojom/ax_node_data.mojom-shared.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {
// The width of a single color focus ring, in density-independent pixels.
constexpr int kFocusRingSingleColorWidthDp = 2;
// Additional buffer needed to prevent clipping at the focus ring's edges.
constexpr int kFocusRingBufferDp = 1;

constexpr int kRadiusDp = 18;
}  // namespace

SwitchAccessBackButtonView::SwitchAccessBackButtonView(bool for_menu)
    : back_button_(new FloatingMenuButton(
          this,
          for_menu ? kSwitchAccessCloseIcon : kSwitchAccessBackIcon,
          IDS_ASH_SWITCH_ACCESS_BACK_BUTTON_DESCRIPTION,
          /*flip_for_rtl=*/false,
          2 * kRadiusDp,
          /*draw_highlight=*/true,
          /*is_a11y_togglable=*/false)) {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets()));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  AddChildView(back_button_);

  // Calculate the side length of the bounding box, with room for the two-color
  // focus ring on either side.
  int focus_ring_width_per_side =
      2 * kFocusRingSingleColorWidthDp + kFocusRingBufferDp;
  int side_length = 2 * (kRadiusDp + focus_ring_width_per_side);
  gfx::Size size(side_length, side_length);
  SetSize(size);
}

void SwitchAccessBackButtonView::SetFocusRing(bool should_show) {
  if (show_focus_ring_ == should_show)
    return;
  show_focus_ring_ = should_show;
  SchedulePaint();
}

void SwitchAccessBackButtonView::SetForMenu(bool for_menu) {
  if (for_menu)
    back_button_->SetVectorIcon(kSwitchAccessCloseIcon);
  else
    back_button_->SetVectorIcon(kSwitchAccessBackIcon);
}

void SwitchAccessBackButtonView::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  NotifyAccessibilityEvent(ax::mojom::Event::kClicked,
                           /*send_native_event=*/false);
}

void SwitchAccessBackButtonView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
}

int SwitchAccessBackButtonView::GetHeightForWidth(int w) const {
  return w;
}

const char* SwitchAccessBackButtonView::GetClassName() const {
  return "SwitchAccessBackButtonView";
}

void SwitchAccessBackButtonView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(gfx::kGoogleGrey800);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), kRadiusDp, flags);

  if (!show_focus_ring_)
    return;

  flags.setColor(gfx::kGoogleBlue300);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kFocusRingSingleColorWidthDp);
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()),
                     kRadiusDp + kFocusRingSingleColorWidthDp, flags);

  flags.setColor(SK_ColorBLACK);
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()),
                     kRadiusDp + (2 * kFocusRingSingleColorWidthDp), flags);
}

}  // namespace ash
