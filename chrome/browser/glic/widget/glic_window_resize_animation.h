// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_RESIZE_ANIMATION_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_RESIZE_ANIMATION_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/rect_f.h"

namespace glic {

class GlicWidget;
class GlicWindowAnimator;

// This class controls the animation of the glic window from one size to
// another. It has the following constraints that the caller must enforce:
// * The glic window and glic view must outlive instances of this class.
// * There can be at most 1 animation at any point in time.
// * Callbacks are posted when this object is destroyed and must remain valid
// until they run. This class will generally override any other changes to
// window size.
// This class deals exclusively with the widget bounds, which may be different
// than the content bounds on Windows.
class GlicWindowResizeAnimation : public gfx::LinearAnimation,
                                  public gfx::AnimationDelegate {
 public:
  GlicWindowResizeAnimation(base::WeakPtr<GlicWidget> widget,
                            GlicWindowAnimator* window_animator,
                            const gfx::Rect& target_bounds,
                            base::TimeDelta duration,
                            base::OnceClosure destruction_callback);
  GlicWindowResizeAnimation(const GlicWindowResizeAnimation&) = delete;
  GlicWindowResizeAnimation& operator=(const GlicWindowResizeAnimation&) =
      delete;
  ~GlicWindowResizeAnimation() override;

  base::TimeDelta duration_left() const { return duration_left_; }

  gfx::Rect target_bounds() const { return new_bounds_; }

  void AnimateToState(double state) override;
  void AnimationEnded(const Animation* animation) override;

  // Change the target bounds only and add `callback` to the list of callbacks
  // to be run on destruction.
  void UpdateTargetBounds(const gfx::Rect& target_bounds,
                          base::OnceClosure callback);

 private:
  // GlicWindowAnimator owns GlicWindowResizeAnimation
  // and will outlive it
  base::WeakPtr<GlicWidget> widget_;
  const raw_ptr<GlicWindowAnimator> glic_window_animator_;
  const gfx::Rect initial_bounds_;
  gfx::Rect new_bounds_;
  base::TimeDelta duration_left_;
  std::unique_ptr<base::OnceClosureList> destruction_callbacks_;
  base::WeakPtrFactory<GlicWindowResizeAnimation> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_RESIZE_ANIMATION_H_
