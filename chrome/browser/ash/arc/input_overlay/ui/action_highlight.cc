// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_highlight.h"

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

namespace {

// ActionHighlight specs.
constexpr int kActionTapCircleRadius = 37;
constexpr int kActionMoveCircleRadius = 69;
constexpr int kCircleStrokeThickness = 2;

}  // namespace

ActionHighlight::ActionHighlight(DisplayOverlayController* controller,
                                 ActionView* anchor_view)
    : controller_(controller), anchor_view_(anchor_view) {
  observation_.Observe(anchor_view);
}

ActionHighlight::~ActionHighlight() {
  observation_.Reset();
}

void ActionHighlight::UpdateAnchorView(ActionView* anchor_view) {
  if (anchor_view == anchor_view_) {
    return;
  }

  observation_.Reset();
  observation_.Observe(anchor_view);
  anchor_view_ = anchor_view;

  UpdateWidgetBounds();
}

void ActionHighlight::OnViewRemovedFromWidget(views::View*) {
  controller_->RemoveActionHighlightWidget();
}

void ActionHighlight::UpdateWidgetBounds() {
  auto* widget = GetWidget();
  DCHECK(widget);

  const int overall_radius = GetOverallRadius();
  auto origin_pos =
      anchor_view_->touch_point()->GetBoundsInScreen().CenterPoint();
  origin_pos.Offset(-overall_radius, -overall_radius);
  widget->SetBounds(gfx::Rect(origin_pos, GetPreferredSize()));
}

int ActionHighlight::GetCircleRadius() const {
  switch (anchor_view_->action()->GetType()) {
    case ActionType::TAP:
      return kActionTapCircleRadius;
    case ActionType::MOVE:
      return kActionMoveCircleRadius;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

int ActionHighlight::GetOverallRadius() const {
  return GetCircleRadius() + kCircleStrokeThickness;
}

gfx::Size ActionHighlight::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int side = 2 * GetOverallRadius();
  return gfx::Size(side, side);
}

void ActionHighlight::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  const int overall_radius = GetOverallRadius();
  const auto center = gfx::Point(overall_radius, overall_radius);

  // Draw circle ring border.
  const int radius = GetCircleRadius();
  ui::ColorProvider* color_provider = GetColorProvider();
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kCircleStrokeThickness);
  flags.setColor(color_provider->GetColor(cros_tokens::kCrosSysPrimary));
  canvas->DrawCircle(center, radius, flags);
  // Draw circle.
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(
      SkColorSetA(color_provider->GetColor(cros_tokens::kCrosSysPrimary),
                  GetAlpha(/*percent=*/0.6f)));
  canvas->DrawCircle(center, radius, flags);
}

void ActionHighlight::VisibilityChanged(views::View* starting_from,
                                        bool is_visible) {
  if (is_visible) {
    UpdateWidgetBounds();
  }
}

BEGIN_METADATA(ActionHighlight)
END_METADATA

}  // namespace arc::input_overlay
