// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_AUTOCLICK_AUTOCLICK_SCROLL_POSITION_HANDLER_H_
#define ASH_ACCESSIBILITY_AUTOCLICK_AUTOCLICK_SCROLL_POSITION_HANDLER_H_

#include <memory>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace ash {

// AutoclickScrollPositionHandler displays the position at which the next scroll
// event will occur, giving users a sense of which part of the screen will
// receive scroll events. It will display at full opacity for a short time, then
// partially fade out to keep from blocking content.
class AutoclickScrollPositionHandler : public gfx::AnimationDelegate {
 public:
  explicit AutoclickScrollPositionHandler(
      std::unique_ptr<views::Widget> widget);
  AutoclickScrollPositionHandler(const AutoclickScrollPositionHandler&) =
      delete;
  AutoclickScrollPositionHandler& operator=(
      const AutoclickScrollPositionHandler&) = delete;
  ~AutoclickScrollPositionHandler() override;

  gfx::NativeView GetNativeView();

  void SetScrollPointCenterInScreen(const gfx::Point& scroll_point_center);

 private:
  static constexpr auto kOpaqueTime = base::Milliseconds(500);
  static constexpr auto kFadeTime = base::Milliseconds(500);

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  std::unique_ptr<views::Widget> widget_;

  // Animation that fades the scroll indicator from full to partial opacity.
  gfx::LinearAnimation animation_{
      kFadeTime, gfx::LinearAnimation::kDefaultFrameRate, this};

  // Timer that keeps the indicator at full opacity briefly after updating.
  base::DelayTimer timer_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_AUTOCLICK_AUTOCLICK_SCROLL_POSITION_HANDLER_H_
