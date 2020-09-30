// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/grid.h"

#include "ash/hud_display/hud_constants.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_constants.h"

namespace ash {
namespace hud_display {

namespace {

constexpr SkColor kGridColor = SkColorSetRGB(162, 162, 220);

base::string16 GenerateLabelText(float value, const base::string16& dimention) {
  if (value == (int)value) {
    return base::ASCIIToUTF16(base::StringPrintf("%d ", (int)value).c_str()) +
           dimention;
  } else {
    return base::ASCIIToUTF16(base::StringPrintf("%.2f ", value).c_str()) +
           dimention;
  }
}

}  // anonymous namespace

BEGIN_METADATA(Grid, views::View)
END_METADATA

// Grid is not transparent.
Grid::Grid(float left,
           float top,
           float right,
           float bottom,
           const base::string16& x_unit,
           const base::string16& y_unit,
           int horizontal_points_number,
           int horizontal_ticks_interval)
    : color_(kGridColor),
      left_(left),
      top_(top),
      right_(right),
      bottom_(bottom),
      x_unit_(x_unit),
      y_unit_(y_unit),
      horizontal_points_number_(horizontal_points_number),
      horizontal_ticks_interval_(horizontal_ticks_interval) {
  // not implemented.
  ALLOW_UNUSED_LOCAL(right_);

  // Text is set later.
  right_top_label_ = AddChildView(std::make_unique<views::Label>(
      base::string16(), views::style::CONTEXT_LABEL));
  right_middle_label_ = AddChildView(std::make_unique<views::Label>(
      base::string16(), views::style::CONTEXT_LABEL));
  right_bottom_label_ = AddChildView(std::make_unique<views::Label>(
      base::string16(), views::style::CONTEXT_LABEL));
  left_bottom_label_ = AddChildView(std::make_unique<views::Label>(
      base::string16(), views::style::CONTEXT_LABEL));

  // Set label text.
  SetTopLabel(top_);
  SetBottomLabel(bottom_);
  SetLeftLabel(left_);

  right_top_label_->SetEnabledColor(color_);
  right_top_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  right_middle_label_->SetEnabledColor(color_);
  right_middle_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  right_bottom_label_->SetEnabledColor(color_);
  right_bottom_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  left_bottom_label_->SetEnabledColor(color_);
  left_bottom_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

Grid::~Grid() = default;

void Grid::Layout() {
  // Align all the right labels on their left edge.
  gfx::Size right_top_label_size = right_top_label_->GetPreferredSize();
  gfx::Size right_middle_label_size = right_middle_label_->GetPreferredSize();
  gfx::Size right_bottom_label_size = right_bottom_label_->GetPreferredSize();

  const int right_labels_width = std::max(
      right_top_label_size.width(), std::max(right_middle_label_size.width(),
                                             right_bottom_label_size.width()));
  right_top_label_size.set_width(right_labels_width);
  right_middle_label_size.set_width(right_labels_width);
  right_bottom_label_size.set_width(right_labels_width);

  right_top_label_->SetSize(right_top_label_size);
  right_middle_label_->SetSize(right_middle_label_size);
  right_bottom_label_->SetSize(right_bottom_label_size);

  left_bottom_label_->SetSize(left_bottom_label_->GetPreferredSize());

  constexpr int label_border = 3;  // Offset to labels from the grid lines.

  const gfx::Point right_top_label_position(
      bounds().width() - right_top_label_size.width() - label_border,
      label_border);
  const gfx::Point right_middle_label_position(
      bounds().width() - right_middle_label_size.width() - label_border,
      bounds().height() / 2 - right_middle_label_size.height() - label_border);
  const gfx::Point right_bottom_label_position(
      bounds().width() - right_bottom_label_size.width() - label_border,
      bounds().height() - right_bottom_label_size.height() - label_border);

  right_top_label_->SetPosition(right_top_label_position);
  right_middle_label_->SetPosition(right_middle_label_position);
  right_bottom_label_->SetPosition(right_bottom_label_position);

  left_bottom_label_->SetPosition(
      {label_border, bounds().height() -
                         left_bottom_label_->GetPreferredSize().height() -
                         label_border});

  views::View::Layout();
}

void Grid::OnPaint(gfx::Canvas* canvas) {
  SkPath dotted_path;
  SkPath solid_path;

  // Draw 50% dotted line.
  dotted_path.moveTo({0, bounds().height() / 2});
  dotted_path.lineTo({bounds().width(), bounds().height() / 2});

  // Draw outside rectangle and ticks
  solid_path.addRect(SkRect::MakeXYWH(bounds().x(), bounds().y(),
                                      bounds().width(), bounds().height()));

  const SkScalar tick_length = 3;

  // Vertical interval ticks (drawn horizontally).
  constexpr int v_ticks = 10;  // Draw 10 vertical intervals between 0 and 100%.
  for (int i = 1; i < v_ticks; ++i) {
    // Skip 50%.
    if (i == v_ticks / 2)
      continue;

    const SkScalar line_y = bounds().height() / (float)v_ticks * i;
    solid_path.moveTo({0, line_y});
    solid_path.lineTo({tick_length, line_y});

    solid_path.moveTo({bounds().width() - tick_length, line_y});
    solid_path.lineTo({bounds().width(), line_y});
  }

  // Horizontal interval ticks (drawn vertically).
  if (horizontal_points_number_ > 0 && horizontal_ticks_interval_ > 0) {
    // Add one more tick if graph width is not a multiply of tick width.
    const int h_ticks =
        horizontal_points_number_ / horizontal_ticks_interval_ +
        (horizontal_points_number_ % horizontal_ticks_interval_ ? 1 : 0);
    // interval between ticks in pixels.
    const SkScalar tick_per_pixels = bounds().width() /
                                     (float)horizontal_points_number_ *
                                     horizontal_ticks_interval_;
    for (int i = 1; i < h_ticks; ++i) {
      const SkScalar line_x = bounds().width() - tick_per_pixels * i;
      solid_path.moveTo({line_x, 0});
      solid_path.lineTo({line_x, tick_length});

      solid_path.moveTo({line_x, bounds().height() - tick_length});
      solid_path.lineTo({line_x, bounds().height()});
    }
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kGridLineWidth);
  flags.setColor(color_);
  canvas->DrawPath(solid_path, flags);

  const SkScalar intervals[] = {5, 3};
  flags.setPathEffect(SkDashPathEffect::Make(
      intervals, sizeof(intervals) / sizeof(intervals[0]), /*phase=*/0));
  canvas->DrawPath(dotted_path, flags);
}

void Grid::SetTopLabel(float top) {
  top_ = top;
  right_top_label_->SetText(GenerateLabelText(top_, y_unit_));
  right_middle_label_->SetText(
      GenerateLabelText((top_ - bottom_) / 2, y_unit_));

  // This might trigger label resize.
  InvalidateLayout();
}

void Grid::SetBottomLabel(float bottom) {
  bottom_ = bottom;
  right_bottom_label_->SetText(GenerateLabelText(bottom_, y_unit_));
  right_middle_label_->SetText(
      GenerateLabelText((top_ - bottom_) / 2, y_unit_));

  // This might trigger label resize.
  InvalidateLayout();
}

void Grid::SetLeftLabel(float left) {
  left_ = left;
  left_bottom_label_->SetText(GenerateLabelText(left_, x_unit_));

  // This might trigger label resize.
  InvalidateLayout();
}

}  // namespace hud_display
}  // namespace ash
