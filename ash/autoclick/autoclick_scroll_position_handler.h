// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTOCLICK_AUTOCLICK_SCROLL_POSITION_HANDLER_H_
#define ASH_AUTOCLICK_AUTOCLICK_SCROLL_POSITION_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"

namespace ash {

class AutoclickScrollPositionView;

// AutoclickScrollPositionHandler displays the position at which the next scroll
// event will occur, giving users a sense of which part of the screen will
// receive scroll events. It will display at full opacity for a short time, then
// partially fade out to keep from blocking content.
class AutoclickScrollPositionHandler : public gfx::LinearAnimation {
 public:
  AutoclickScrollPositionHandler(const gfx::Point& center_point_in_screen,
                                 views::Widget* widget);
  ~AutoclickScrollPositionHandler() override;

  void SetCenter(const gfx::Point& center_point_in_screen,
                 views::Widget* widget);

 private:
  enum AnimationState {
    kWait,
    kFade,
    kDone,
  };

  // Overridden from gfx::LinearAnimation.
  void AnimateToState(double state) override;
  void AnimationStopped() override;

  std::unique_ptr<AutoclickScrollPositionView> view_;
  AnimationState animation_state_ = kDone;

  DISALLOW_COPY_AND_ASSIGN(AutoclickScrollPositionHandler);
};

}  // namespace ash

#endif  // ASH_AUTOCLICK_AUTOCLICK_SCROLL_POSITION_HANDLER_H_
