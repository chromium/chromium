// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_up_next_view_background_painter.h"

#include "cc/paint/paint_flags.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr int kBackgroundCornerRadius = 24;
// Space for the top point of the background.
// We start drawing a rectangle 12dip off the top and then eventually curve up
// when drawing the circle. The top of the circle will be at y: 0.
constexpr float kTopOffset = 12.f;

}  // namespace

CalendarUpNextViewBackground::CalendarUpNextViewBackground(ui::ColorId color_id)
    : color_id_(color_id) {}

CalendarUpNextViewBackground::~CalendarUpNextViewBackground() = default;

SkPath CalendarUpNextViewBackground::GetPath(const gfx::Size& size) {
  // First draw a rounded rectangle.
  SkPath path;
  gfx::RectF rect_f((gfx::SizeF(size)));
  rect_f.set_y(kTopOffset);
  SkRect rect = SkRect{rect_f.x(), rect_f.y(), rect_f.width(), rect_f.height()};
  path.addRoundRect(
      rect, (SkScalar[]){kBackgroundCornerRadius, kBackgroundCornerRadius,
                         kBackgroundCornerRadius, kBackgroundCornerRadius, 0.f,
                         0.f, 0.f, 0.f});
  path.close();

  // Cache center-x for positioning curves when size changes.
  const float cx = rect_f.CenterPoint().x();

  // y values are shared between both curves.
  const float curve_bottom_y = kTopOffset;
  const float curve_top_y = 7;
  const float curve_control_point_y = 12.84f;

  // Draw left curve.
  const float left_curve_start_x = cx - 23.f;
  const float left_curve_end_x = cx - 13.f;
  const float left_curve_control_point_x = cx - 16.86f;
  path.moveTo(left_curve_start_x, curve_bottom_y);
  path.cubicTo(left_curve_start_x, curve_bottom_y, left_curve_control_point_x,
               curve_control_point_y, left_curve_end_x, curve_top_y);
  path.lineTo(left_curve_end_x, curve_bottom_y);
  path.close();

  // Draw right curve.
  const float right_curve_start_x = cx + 13.f;
  const float right_curve_end_x = cx + 23.f;
  const float right_curve_control_point_x = cx + 16.86f;
  path.moveTo(right_curve_start_x, curve_bottom_y);
  path.lineTo(right_curve_start_x, curve_top_y);
  path.cubicTo(right_curve_start_x, curve_top_y, right_curve_control_point_x,
               curve_control_point_y, right_curve_end_x, curve_bottom_y);
  path.close();

  // Draw circle in the center.
  constexpr float kRadius = 16.f;
  path.addCircle(cx, /*y=*/kRadius, kRadius);
  path.close();

  return path;
}

void CalendarUpNextViewBackground::Paint(gfx::Canvas* canvas,
                                         views::View* view) const {
  // Setup paint.
  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrcOver);
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(view->GetColorProvider()->GetColor(color_id_));

  // Get the path to draw on the canvas.
  SkPath path = GetPath(view->GetLocalBounds().size());

  // Draw the path.
  canvas->DrawPath(path, flags);
}

void CalendarUpNextViewBackground::OnViewThemeChanged(views::View* view) {
  SetNativeControlColor(view->GetColorProvider()->GetColor(color_id_));
  view->SchedulePaint();
}

}  // namespace ash
