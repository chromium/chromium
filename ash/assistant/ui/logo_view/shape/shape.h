// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_LOGO_VIEW_SHAPE_SHAPE_H_
#define ASH_ASSISTANT_UI_LOGO_VIEW_SHAPE_SHAPE_H_

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"

namespace ash {

// A Shape is basically a Path with information on the stroke width and cap
// type. A Shape can consist of two Paths that can have different stroke widths.
// MicPartShape only has the |first_path|. LetterShape has two Paths.
class Shape {
 public:
  explicit Shape(float dot_size);

  Shape(const Shape&) = delete;
  Shape& operator=(const Shape&) = delete;

  virtual ~Shape();

  float dot_size() const { return dot_size_; }

  SkPath* first_path() { return &first_path_; }

  SkPath* second_path() { return &second_path_; }

  void set_first_stroke_width(float first_stroke_width) {
    first_stroke_width_ = first_stroke_width;
  }
  float first_stroke_width() const { return first_stroke_width_; }

  void set_second_stroke_width(float second_stroke_width) {
    second_stroke_width_ = second_stroke_width;
  }
  float second_stroke_width() const { return second_stroke_width_; }

  void set_cap(cc::PaintFlags::Cap cap) { cap_ = cap; }
  cc::PaintFlags::Cap cap() const { return cap_; }

  void Reset();

  void Transform(float x, float y, float scale);

 private:
  const float dot_size_;

  SkPath first_path_;
  SkPath second_path_;

  float first_stroke_width_ = 0.0f;
  float second_stroke_width_ = 0.0f;

  cc::PaintFlags::Cap cap_ = cc::PaintFlags::kRound_Cap;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_LOGO_VIEW_SHAPE_SHAPE_H_
