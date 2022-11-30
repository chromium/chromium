// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/logo_view/shape/shape.h"

#include "third_party/skia/include/core/SkMatrix.h"

namespace ash {

Shape::Shape(float dot_size) : dot_size_(dot_size) {}

Shape::~Shape() = default;

void Shape::Reset() {
  first_path_.reset();
  second_path_.reset();
  first_stroke_width_ = 0.0f;
  second_stroke_width_ = 0.0f;
  cap_ = cc::PaintFlags::kRound_Cap;
}

void Shape::Transform(float x, float y, float scale) {
  SkMatrix matrix;
  matrix.reset();
  matrix.preScale(scale, scale);
  matrix.preTranslate(x, y);

  first_path_.transform(matrix);
  second_path_.transform(matrix);

  first_stroke_width_ *= scale;
  second_stroke_width_ *= scale;
}

}  // namespace ash
