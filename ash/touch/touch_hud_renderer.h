// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_HUD_RENDERER_H_
#define ASH_TOUCH_TOUCH_HUD_RENDERER_H_

#include <map>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class TouchEvent;
}

namespace views {
class Widget;
}

namespace ash {

class TouchPointView;

// Handles touch events to draw out touch points for TouchHudProjection
// (--show-taps).
class ASH_EXPORT TouchHudRenderer : public views::WidgetObserver {
 public:
  explicit TouchHudRenderer(views::Widget* parent_widget);
  ~TouchHudRenderer() override;

  // Called to clear touch points and traces from the screen.
  void Clear();

  // Receives a touch event and draws its touch point.
  void HandleTouchEvent(const ui::TouchEvent& event);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  friend class TouchHudProjectionTest;

  // The parent widget that all touch points would be drawn in.
  views::Widget* parent_widget_;

  // A map of touch ids to TouchPointView.
  std::map<int, std::unique_ptr<TouchPointView>> points_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudRenderer);
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_HUD_RENDERER_H_
