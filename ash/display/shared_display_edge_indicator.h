// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SHARED_DISPLAY_EDGE_INDICATOR_H_
#define ASH_DISPLAY_SHARED_DISPLAY_EDGE_INDICATOR_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {
class Rect;
class ThrobAnimation;
}

namespace views {
class Widget;
}

namespace ash {

// SharedDisplayEdgeIndicator is responsible for showing a window that indicates
// the edge that a window can be dragged into another display.
class ASH_EXPORT SharedDisplayEdgeIndicator : public gfx::AnimationDelegate {
 public:
  SharedDisplayEdgeIndicator();
  SharedDisplayEdgeIndicator(const SharedDisplayEdgeIndicator&) = delete;
  SharedDisplayEdgeIndicator& operator=(const SharedDisplayEdgeIndicator&) =
      delete;
  ~SharedDisplayEdgeIndicator() override;

  // Shows the indicator window. The |src_bounds| is for the window on the
  // source display, and the |dst_bounds| is for the window on the other
  // display. Hiding is done in the destructor, when the widgets get released.
  void Show(const gfx::Rect& src_bounds, const gfx::Rect& dst_bounds);

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  // Used to show the displays' shared edge where a window can be moved across.
  // |src_widget_| is for the display where drag starts and |dst_widget_| is
  // for the other display.
  std::unique_ptr<views::Widget> src_widget_;
  std::unique_ptr<views::Widget> dst_widget_;

  // Used to transition the opacity.
  std::unique_ptr<gfx::ThrobAnimation> animation_;
};

}  // namespace ash

#endif  // ASH_DISPLAY_SHARED_DISPLAY_EDGE_INDICATOR_H_
