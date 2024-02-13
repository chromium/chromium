// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_REFERENCE_LINES_H_
#define ASH_HUD_DISPLAY_REFERENCE_LINES_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}

namespace views {
class View;
class Label;
}  // namespace views

namespace ash {
namespace hud_display {

// Draws opaque reference lines on top of the graphs.
class ReferenceLines : public views::View {
  METADATA_HEADER(ReferenceLines, views::View)

 public:
  // |left|, |top|, |right|, |bottom| are labels to be attached to the axes.
  // |x_unit|, |y_unit| - dimentional labels, like "s", "Gb", ...
  // To draw horizontal ticks, graph data is assumed to have
  // |horizontal_points_number| points horizontally along the full graph width,
  // and ticks will be drawn every |horizontal_ticks_interval| from the right.
  // |vertical_ticks_interval| must be in between [0,1] (as graph data values).
  ReferenceLines(float left,
                 float top,
                 float right,
                 float bottom,
                 const std::u16string& x_unit,
                 const std::u16string& y_unit,
                 int horizontal_points_number,
                 int horizontal_ticks_interval,
                 float vertical_ticks_interval);

  ReferenceLines(const ReferenceLines&) = delete;
  ReferenceLines& operator=(const ReferenceLines&) = delete;

  ~ReferenceLines() override;

  // views::View
  void Layout(PassKey) override;
  void OnPaint(gfx::Canvas* canvas) override;

  // The following methods update reference line parameters.
  void SetTopLabel(float top);
  void SetBottomLabel(float bottom);
  void SetLeftLabel(float left);
  void SetVerticalTicksInterval(float interval);

  float top_label() const { return top_; }

 private:
  const SkColor color_;

  // Graph label values. Note that `right_` is not implemented, thus the
  // [[maybe_unused]].
  float left_ = 0;
  float top_ = 0;
  [[maybe_unused]] float right_ = 0;
  float bottom_ = 0;

  std::u16string x_unit_;
  std::u16string y_unit_;

  // horizontal ticks
  int horizontal_points_number_ = 0;
  int horizontal_ticks_interval_ = 0;
  float vertical_ticks_interval_ = 0;

  // Graph labels
  raw_ptr<views::Label> right_top_label_ = nullptr;     // not owned
  raw_ptr<views::Label> right_middle_label_ = nullptr;  // not owned
  raw_ptr<views::Label> right_bottom_label_ = nullptr;  // not owned
  raw_ptr<views::Label> left_bottom_label_ = nullptr;   // not owned
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_REFERENCE_LINES_H_
