// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ROUNDED_LABEL_WIDGET_H_
#define ASH_STYLE_ROUNDED_LABEL_WIDGET_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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
    ~InitParams();

    std::string name;
    int horizontal_padding;
    int vertical_padding;
    int rounding_dp;
    int preferred_height;
    // A message string or the string ID.
    // TODO(zxdan): change back to message ID if test string is no longer
    // needed.
    absl::variant<std::u16string, int> message;
    raw_ptr<aura::Window> parent;
    bool disable_default_visibility_animation = false;
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
