// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_WINDOW_RESIZE_ANIMATION_H_
#define CHROME_BROWSER_GLIC_GLIC_WINDOW_RESIZE_ANIMATION_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/rect_f.h"

namespace views {
class Widget;
}  // namespace views

namespace glic {

// This class controls the animation of the glic window from one size to
// another. It has the following constraints that the caller must enforce:
// * The glic window and glic view must outlive instances of this class.
// * There can be at most 1 animation at any point in time.
// This class will generally override any other changes to window size.
class GlicWindowResizeAnimation : public gfx::LinearAnimation,
                                  public gfx::AnimationDelegate {
 public:
  // The caller is expected to destroy GlicWindowResizeAnimation upon receiving
  // FinishedCallback. FinishedCallback is always invoked asynchronously.
  using FinishedCallback = base::OnceClosure;
  GlicWindowResizeAnimation(views::Widget* widget,
                            const gfx::Rect& target_bounds,
                            base::TimeDelta duration,
                            FinishedCallback finished_callback);
  GlicWindowResizeAnimation(const GlicWindowResizeAnimation&) = delete;
  GlicWindowResizeAnimation& operator=(const GlicWindowResizeAnimation&) =
      delete;
  ~GlicWindowResizeAnimation() override;

  void AnimateToState(double state) override;
  void AnimationEnded(const Animation* animation) override;

 private:
  const raw_ptr<views::Widget> widget_;
  const gfx::Rect initial_bounds_;
  const gfx::Rect new_bounds_;
  FinishedCallback finished_callback_;
  base::WeakPtrFactory<GlicWindowResizeAnimation> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_WINDOW_RESIZE_ANIMATION_H_
