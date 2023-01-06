// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_WINDOW_SCALE_ANIMATION_H_
#define ASH_SHELF_WINDOW_SCALE_ANIMATION_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"

namespace ash {

enum class BackdropWindowMode;

// The class which does the scale-down animation to shelf or scale-up to restore
// to its original bounds for all windows in the transient tree of |window_|
// after drag ends. Window(s) will be minimized with the descending order
// in the transient tree after animation completes if we're scaling down to
// shelf.
class ASH_EXPORT WindowScaleAnimation {
 public:
  enum class WindowScaleType {
    kScaleDownToShelf,
    kScaleUpToRestore,
  };

  WindowScaleAnimation(WindowScaleType scale_type,
                       base::OnceClosure opt_callback);

  WindowScaleAnimation(const WindowScaleAnimation&) = delete;
  WindowScaleAnimation& operator=(const WindowScaleAnimation&) = delete;

  ~WindowScaleAnimation();

  // Starts animating and creating animation observers for all window(s) in the
  // transient tree of `window` in a descending order,
  void Start(aura::Window* window);

  // For tests only:
  static base::AutoReset<bool>
  EnableScopedFastAnimationForTransientChildForTest();

 private:
  class AnimationObserver;

  void DestroyWindowAnimationObserver(
      WindowScaleAnimation::AnimationObserver* animation_observer);

  // `window` is the last window in the transient tree to complete its
  // animation.
  void OnScaleWindowsOnAnimationsCompleted(aura::Window* window);

  base::OnceClosure opt_callback_;

  const WindowScaleType scale_type_;

  // Each window in the transient tree has its own `WindowAnimationObserver`.
  std::vector<std::unique_ptr<AnimationObserver>> window_animation_observers_;
};

}  // namespace ash

#endif  // ASH_SHELF_WINDOW_SCALE_ANIMATION_H_
