// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ROUNDED_LABEL_WIDGET_H_
#define ASH_STYLE_ROUNDED_LABEL_WIDGET_H_

#include <string>

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
    int rounding_dp;
    int preferred_height;
    int message_id;
    aura::Window* parent;
    bool hide_in_mini_view;
  };

  RoundedLabelWidget();
  RoundedLabelWidget(const RoundedLabelWidget&) = delete;
  RoundedLabelWidget& operator=(const RoundedLabelWidget&) = delete;
  ~RoundedLabelWidget() override;

  void Init(InitParams params);

  // Gets the preferred size of the widget centered in |bounds|.
  gfx::Rect GetBoundsCenteredIn(const gfx::Rect& bounds);

  // Places the widget in the middle of |bounds_in_screen|. The size will be the
  // preferred size of the label. If |animate| is true, the widget will be
  // animated to the new bounds.
  void SetBoundsCenteredIn(const gfx::Rect& bounds_in_screen, bool animate);
};

}  // namespace ash

#endif  // ASH_STYLE_ROUNDED_LABEL_WIDGET_H_
