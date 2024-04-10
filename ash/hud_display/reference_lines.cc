// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/reference_lines.h"

#include "ash/hud_display/hud_constants.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_constants.h"

namespace ash {
namespace hud_display {

namespace {

constexpr SkColor kHUDGraphReferenceLineColor = SkColorSetRGB(162, 162, 220);

std::u16string GenerateLabelText(float value, const std::u16string& dimention) {
  if (value == (int)value) {
    return base::ASCIIToUTF16(base::StringPrintf("%d ", (int)value).c_str()) +
           dimention;
  } else {
    return base::ASCIIToUTF16(base::StringPrintf("%.2f ", value).c_str()) +
           dimention;
  }
}

}  // anonymous namespace

BEGIN_METADATA(ReferenceLines)
END_METADATA

ReferenceLines::ReferenceLines(float left,
                               float top,
                               float right,
                               float bottom,
                               const std::u16string& x_unit,
                               const std::u16string& y_unit,
                               int horizontal_points_number,
                               int horizontal_ticks_interval,
                               float vertical_ticks_interval)
    : color_(kHUDGraphReferenceLineColor),
      left_(left),
      top_(top),
      right_(right),
      bottom_(bottom),
      x_unit_(x_unit),
      y_unit_(y_unit),
      horizontal_points_number_(horizontal_points_number),
      horizontal_ticks_interval_(horizontal_ticks_interval),
      vertical_ticks_interval_(vertical_ticks_interval) {
  // Text is set later.
  right_top_label_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL));
  right_middle_label_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL));
  right_bottom_label_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL));
  left_bottom_label_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL));

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

ReferenceLines::~ReferenceLines() = default;

void ReferenceLines::Layout(PassKey) {
  // Align all the right labels on their left edge.
  gfx::Size right_top_label_size = right_top_label_->GetPreferredSize(
      views::SizeBounds(right_top_label_->width(), {}));
  gfx::Size right_middle_label_size = right_middle_label_->GetPreferredSize(
      views::SizeBounds(right_middle_label_->width(), {}));
  gfx::Size right_bottom_label_size = right_bottom_label_->GetPreferredSize(
      views::SizeBounds(right_bottom_label_->width(), {}));

  const int right_labels_width = std::max(
      right_top_label_size.width(), std::max(right_middle_label_size.width(),
                                             right_bottom_label_size.width()));
  right_top_label_size.set_width(right_labels_width);
  right_middle_label_size.set_width(right_labels_width);
  right_bottom_label_size.set_width(right_labels_width);

  right_top_label_->SetSize(right_top_label_size);
  right_middle_label_->SetSize(right_middle_label_size);
  right_bottom_label_->SetSize(right_bottom_label_size);

  left_bottom_label_->SetSize(left_bottom_label_->GetPreferredSize(
      views::SizeBounds(left_bottom_label_->width(), {})));

  constexpr int label_border = 3;  // Offset to labels from the reference lines.

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
                         left_bottom_label_
                             ->GetPreferredSize(views::SizeBounds(
                                 left_bottom_label_->width(), {}))
                             .height() -
                         label_border});

  LayoutSuperclass<views::View>(this);
}

void ReferenceLines::OnPaint(gfx::Canvas* canvas) {
  SkPath dotted_path;
  SkPath solid_path;

  // Draw dashed line at 50%.
  dotted_path.moveTo({0, bounds().height() / 2.0f});
  dotted_path.lineTo(
      {static_cast<SkScalar>(bounds().width()), bounds().height() / 2.0f});

  // Draw border and ticks.
  solid_path.addRect(SkRect::MakeXYWH(bounds().x(), bounds().y(),
                                      bounds().width(), bounds().height()));

  const SkScalar tick_length = 3;

  // Vertical interval ticks (drawn horizontally).
  if (vertical_ticks_interval_ > 0) {
    float tick_bottom_offset = vertical_ticks_interval_;
    while (tick_bottom_offset <= 1) {
      // Skip 50%.
      if (fabs(tick_bottom_offset - .5) > 0.01) {
        const SkScalar line_y = (1 - tick_bottom_offset) * bounds().height();
        solid_path.moveTo({0, line_y});
        solid_path.lineTo({tick_length, line_y});

        solid_path.moveTo({bounds().width() - tick_length, line_y});
        solid_path.lineTo({static_cast<SkScalar>(bounds().width()), line_y});
      }
      tick_bottom_offset += vertical_ticks_interval_;
    }
  }

  // Horizontal interval ticks (drawn vertically).
  if (horizontal_points_number_ > 0 && horizontal_ticks_interval_ > 0) {
    // Add one more tick if graph width is not a multiple of tick width.
    const int h_ticks =
        horizontal_points_number_ / horizontal_ticks_interval_ +
        (horizontal_points_number_ % horizontal_ticks_interval_ ? 1 : 0);
    // Interval between ticks in pixels.
    const SkScalar tick_per_pixels = bounds().width() /
                                     (float)horizontal_points_number_ *
                                     horizontal_ticks_interval_;
    for (int i = 1; i < h_ticks; ++i) {
      const SkScalar line_x = bounds().width() - tick_per_pixels * i;
      solid_path.moveTo({line_x, 0});
      solid_path.lineTo({line_x, tick_length});

      solid_path.moveTo({line_x, bounds().height() - tick_length});
      solid_path.lineTo({line_x, static_cast<SkScalar>(bounds().height())});
    }
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kHUDGraphReferenceLineWidth);
  flags.setColor(color_);
  canvas->DrawPath(solid_path, flags);

  const SkScalar intervals[] = {5, 3};
  flags.setPathEffect(cc::PathEffect::MakeDash(
      intervals, sizeof(intervals) / sizeof(intervals[0]), /*phase=*/0));
  canvas->DrawPath(dotted_path, flags);
}

void ReferenceLines::SetTopLabel(float top) {
  top_ = top;
  right_top_label_->SetText(GenerateLabelText(top_, y_unit_));
  right_middle_label_->SetText(
      GenerateLabelText((top_ - bottom_) / 2, y_unit_));

  // This might trigger label resize.
  InvalidateLayout();
}

void ReferenceLines::SetBottomLabel(float bottom) {
  bottom_ = bottom;
  right_bottom_label_->SetText(GenerateLabelText(bottom_, y_unit_));
  right_middle_label_->SetText(
      GenerateLabelText((top_ - bottom_) / 2, y_unit_));

  // This might trigger label resize.
  InvalidateLayout();
}

void ReferenceLines::SetLeftLabel(float left) {
  left_ = left;
  left_bottom_label_->SetText(GenerateLabelText(left_, x_unit_));

  // This might trigger label resize.
  InvalidateLayout();
}

void ReferenceLines::SetVerticalTicksInterval(float interval) {
  interval = std::abs(interval) >= 1 ? 0 : std::abs(interval);
  if (interval == vertical_ticks_interval_)
    return;

  vertical_ticks_interval_ = interval;
}

}  // namespace hud_display
}  // namespace ash
