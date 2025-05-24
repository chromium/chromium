// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_ANIMATOR_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_ANIMATOR_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"

namespace glic {

class GlicWindowController;
class GlicWindowResizeAnimation;

// This class will handle the open and close animations for the glic widget
// including referring to GlicWindowResizeAnimation for
// resizing-related animations.
class GlicWindowAnimator : public gfx::AnimationDelegate {
 public:
  explicit GlicWindowAnimator(GlicWindowController* window_controller);
  GlicWindowAnimator(const GlicWindowAnimator&) = delete;
  GlicWindowAnimator& operator=(const GlicWindowAnimator&) = delete;
  ~GlicWindowAnimator() override;

  // Animate the window size, maintaining the position of the top right corner.
  // If there is already a running bounds change animation, update that
  // animation's target size.
  void AnimateSize(const gfx::Size& target_size,
                   base::TimeDelta duration,
                   base::OnceClosure callback);

  // Animate the window's top left position maintaining size. If there is
  // already a running bounds change animation, update that animation's target
  // origin.
  void AnimatePosition(const gfx::Point& target_position,
                       base::TimeDelta duration,
                       base::OnceClosure callback);

  // Called when the programmatic resize has finished. Public for use by
  // GlicWindowResizeAnimation.
  void ResizeFinished();

  // Gets the bounds for the widget's resize animation. If there is an animation
  // already ongoing, use the target bounds for that animation. Otherwise, use
  // the widget's current bounds.
  gfx::Rect GetCurrentTargetBounds();

  // Reset the saved target size.
  void ResetLastTargetSize();

  // If there's a saved target size, start the resize animation for it.
  void MaybeAnimateToTargetSize();

 private:
  // Sets target bounds for the widget (must exist) and creates a
  // GlicWindowResizeAnimation instance to begin a new animation. If a bounds
  // animation is already running, end it and start a new one. Duration is set
  // to 0 if negative.
  void AnimateBounds(const gfx::Rect& target_bounds,
                     base::TimeDelta duration,
                     base::OnceClosure callback);

  // Sets target opacity for the widget (must exist) and creates a
  // GlicWindowOpacityAnimation instance to begin a new opacity animation.
  void AnimateWindowOpacity(float start_opacity,
                            float target_opacity,
                            base::TimeDelta duration);

  // Sets target opacity for the GlicView's web view and creates a
  // GlicViewOpacityAnimation instance to begin a new animation.
  void AnimateViewOpacity(float start_opacity,
                          float target_opacity,
                          base::TimeDelta duration);

  // GlicWindowController owns GlicWindowAnimator and will outlive it
  const raw_ptr<GlicWindowController> window_controller_;
  std::unique_ptr<GlicWindowResizeAnimation> window_resize_animation_;

  // Last requested target size. Will be (0, 0) if there hasn't been a resize
  // request or glic has already been resized to the target size.
  gfx::Size last_target_size_;

  base::WeakPtrFactory<GlicWindowAnimator> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_ANIMATOR_H_
