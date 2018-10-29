// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TAP_VISUALIZER_TAP_RENDERER_H_
#define ASH_COMPONENTS_TAP_VISUALIZER_TAP_RENDERER_H_

#include <map>
#include <memory>

#include "base/macros.h"

namespace ui {
class TouchEvent;
}

namespace views {
class Widget;
}

namespace tap_visualizer {
class TouchPointView;

// Renders touch points into a widget.
class TapRenderer {
 public:
  explicit TapRenderer(std::unique_ptr<views::Widget> widget);
  ~TapRenderer();

  // Receives a touch event and draws its touch point.
  void HandleTouchEvent(const ui::TouchEvent& event);

 private:
  friend class TapVisualizerAppTestApi;

  // The widget containing the touch point views.
  std::unique_ptr<views::Widget> widget_;

  // A map of touch ids to TouchPointView.
  std::map<int, TouchPointView*> points_;

  DISALLOW_COPY_AND_ASSIGN(TapRenderer);
};

}  // namespace tap_visualizer

#endif  // ASH_COMPONENTS_TAP_VISUALIZER_TAP_RENDERER_H_
