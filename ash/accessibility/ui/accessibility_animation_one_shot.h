// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_UI_ACCESSIBILITY_ANIMATION_ONE_SHOT_H_
#define ASH_ACCESSIBILITY_UI_ACCESSIBILITY_ANIMATION_ONE_SHOT_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class Compositor;
}  // namespace ui

namespace ash {

// Adapts the observer-like interface in |CompositorAnimationObserver| for
// simpler, safe usage.
//
// The |Compositor| expects all |CompositorAnimationObserver|s to be added while
// animating, but removed soon after. This class ensures this occurs by
// requiring the user pass a callback rather than dealing with the observer.
class AccessibilityAnimationOneShot : public ui::CompositorAnimationObserver {
 public:
  // |callback| below returns true if animation has finished; false to
  // continue animating.
  AccessibilityAnimationOneShot(
      const gfx::Rect& bounds_in_dip,
      base::RepeatingCallback<bool(base::TimeTicks)> callback);
  ~AccessibilityAnimationOneShot() override;
  AccessibilityAnimationOneShot(const AccessibilityAnimationOneShot&) = delete;
  AccessibilityAnimationOneShot& operator=(
      const AccessibilityAnimationOneShot&) = delete;

 private:
  // CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  base::RepeatingCallback<bool(base::TimeTicks)> callback_;
  base::ScopedObservation<ui::Compositor, ui::CompositorAnimationObserver>
      animation_observation_{this};
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_UI_ACCESSIBILITY_ANIMATION_ONE_SHOT_H_
