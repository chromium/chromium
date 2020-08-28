// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_GRID_H_
#define ASH_HUD_DISPLAY_GRID_H_

#include "base/strings/string16.h"
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

// Draws grid on top of the graphs.
class Grid : public views::View {
 public:
  METADATA_HEADER(Grid);

  // |left|, |top|, |right|, |bottom| are labels to be attached to the axes.
  // |x_unit|, |y_unit| - dimentional labels, like "s", "Gb", ...
  // To draw horizontal ticks, graph data is assumed to have
  // |horizontal_points_number| points horizontally along the full graph width,
  // and ticks will be drawn every |horizontal_ticks_interval| from the right.
  Grid(float left,
       float top,
       float right,
       float bottom,
       const base::string16& x_unit,
       const base::string16& y_unit,
       int horizontal_points_number,
       int horizontal_ticks_interval);

  Grid(const Grid&) = delete;
  Grid& operator=(const Grid&) = delete;

  ~Grid() override;

  // views::View
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;

  // The following methods update grid parameters.
  void SetTopLabel(float top);
  void SetBottomLabel(float bottom);
  void SetLeftLabel(float left);

 private:
  const SkColor color_;

  // Graph label values.
  float left_ = 0;
  float top_ = 0;
  float right_ = 0;
  float bottom_ = 0;

  base::string16 x_unit_;
  base::string16 y_unit_;

  // horizontal ticks
  int horizontal_points_number_ = 0;
  int horizontal_ticks_interval_ = 0;

  // Graph labels
  views::Label* right_top_label_ = nullptr;     // not owned
  views::Label* right_middle_label_ = nullptr;  // not owned
  views::Label* right_bottom_label_ = nullptr;  // not owned
  views::Label* left_bottom_label_ = nullptr;   // not owned
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_GRID_H_
