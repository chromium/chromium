// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/logo_view/shape/mic_part_shape.h"

#include <cmath>

#include "cc/paint/paint_flags.h"
#include "chromeos/assistant/internal/logo_view/logo_model/mic.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace ash {

MicPartShape::MicPartShape(float dot_size)
    : Shape(dot_size), mic_(std::make_unique<chromeos::assistant::Mic>()) {}

MicPartShape::~MicPartShape() = default;

void MicPartShape::ToMicPart(float progress,
                             chromeos::assistant::DotColor dot_color) {
  chromeos::assistant::Mic::MicPartLinePath line_path_down;
  chromeos::assistant::Mic::MicPartArcPath arc_path;
  chromeos::assistant::Mic::MicPartLinePath line_path_up;
  mic_->ToMicPart(progress, dot_color, &line_path_down, &arc_path,
                  &line_path_up);
  first_path()->reset();
  if (line_path_down.is_valid) {
    first_path()->moveTo(SkScalar(line_path_down.start_x),
                         SkScalar(line_path_down.start_y));
    first_path()->lineTo(SkScalar(line_path_down.end_x),
                         SkScalar(line_path_down.end_y));
  }
  if (arc_path.is_valid) {
    first_path()->addArc(
        gfx::RectFToSkRect(gfx::RectF(arc_path.start_x, arc_path.start_y,
                                      arc_path.end_x - arc_path.start_x,
                                      arc_path.end_y - arc_path.start_y)),
        SkScalar(arc_path.start_angle), SkScalar(arc_path.sweep_angle));
  }
  if (line_path_up.is_valid) {
    first_path()->moveTo(SkScalar(line_path_up.start_x),
                         SkScalar(line_path_up.start_y));
    first_path()->lineTo(SkScalar(line_path_up.end_x),
                         SkScalar(line_path_up.end_y));
  }

  float stroke_width;
  cc::PaintFlags::Cap cap = progress > chromeos::assistant::kCapChangeValue
                                ? cc::PaintFlags::kButt_Cap
                                : cc::PaintFlags::kRound_Cap;
  switch (dot_color) {
    case chromeos::assistant::kBlue:
      stroke_width = chromeos::assistant::kBlueMicPartStrokeWidth;
      cap = cc::PaintFlags::kRound_Cap;
      break;
    case chromeos::assistant::kRed:
      stroke_width = chromeos::assistant::kRedMicPartStrokeWidth;
      break;
    case chromeos::assistant::kYellow:
      stroke_width = chromeos::assistant::kYellowMicPartStrokeWidth;
      break;
    case chromeos::assistant::kGreen:
      stroke_width = chromeos::assistant::kGreenMicPartStrokeWidth;
      break;
  }
  set_first_stroke_width(
      gfx::Tween::FloatValueBetween(progress, dot_size(), stroke_width));
  set_cap(cap);
}

}  // namespace ash
