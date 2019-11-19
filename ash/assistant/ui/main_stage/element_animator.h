// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ELEMENT_ANIMATOR_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ELEMENT_ANIMATOR_H_

#include "base/macros.h"
#include "base/time/time.h"

namespace ui {
class CallbackLayerAnimationObserver;
class Layer;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace ash {

// Defines all the animations for the associated UI element.
class ElementAnimator {
 public:
  // Fade out duration used in the default implementation of |FadeOut|.
  constexpr static base::TimeDelta kFadeOutDuration =
      base::TimeDelta::FromMilliseconds(150);
  // Fade out opacity used in the default implementation of |FadeOut|.
  constexpr static float kFadeOutOpacity = 0.26f;
  // Minimum allowed opacity as a target when fading out.
  // Note that we approximate 0% by actually using 0.01%. We do this to
  // workaround a DCHECK that requires aura::Windows to have a target opacity >
  // 0% when shown. Because our window will be removed after it reaches this
  // value, it should be safe to circumnavigate this DCHECK.
  constexpr static float kMinimumAnimateOutOpacity = 0.0001f;

  ElementAnimator(views::View* animated_view);
  virtual ~ElementAnimator() = default;

  // Fade out the current element, meaning it will still be visible but
  // partially opaque.
  virtual void FadeOut(ui::CallbackLayerAnimationObserver* observer);

  // Start the animation to remove the element.
  virtual void AnimateOut(ui::CallbackLayerAnimationObserver* observer) = 0;

  // Start the animation to add the element.
  virtual void AnimateIn(ui::CallbackLayerAnimationObserver* observer) = 0;

  // Abort whatever animation is currently in progress.
  virtual void AbortAnimation();

  // The view that is being animated.
  virtual views::View* view() const;

 protected:
  // The layer that needs to be animated.
  // Used by the default implementations for |FadeOut| and |AbortAnimation|.
  // Defaults to |view()->layer()|.
  virtual ui::Layer* layer() const;

 private:
  // The parent |AnimatedContainerView| owns both |view_| and |this| and will
  // delete |this| when |view_| is removed.
  views::View* const view_;

  DISALLOW_COPY_AND_ASSIGN(ElementAnimator);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ELEMENT_ANIMATOR_H_
