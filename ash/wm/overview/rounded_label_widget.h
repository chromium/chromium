// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_ROUNDED_LABEL_WIDGET_H_
#define ASH_WM_OVERVIEW_ROUNDED_LABEL_WIDGET_H_

#include <string>

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

// RoundedLabelWidget is a subclass of widget which always contains a single
// label view as its child.
class RoundedLabelWidget : public views::Widget {
 public:
  // Params to modify the look of the label.
  struct InitParams {
    InitParams();
    InitParams(InitParams&& other);

    std::string name;
    int horizontal_padding;
    int vertical_padding;
    SkColor background_color;
    SkColor foreground_color;
    int rounding_dp;
    int preferred_height;
    int message_id;
    aura::Window* parent;
    bool hide_in_mini_view;
  };

  RoundedLabelWidget();
  ~RoundedLabelWidget() override;

  void Init(InitParams params);

  // Gets the preferred size of the widget centered in |bounds|.
  gfx::Rect GetBoundsCenteredIn(const gfx::Rect& bounds);

  // Places the widget in the middle of |bounds|. The size will be the preferred
  // size of the label. If |animate| is true, the widget will be animated to the
  // new bounds.
  void SetBoundsCenteredIn(const gfx::Rect& bounds, bool animate);

 private:
  DISALLOW_COPY_AND_ASSIGN(RoundedLabelWidget);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_ROUNDED_LABEL_WIDGET_H_
