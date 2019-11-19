// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_WINDOW_SCALE_ANIMATION_H_
#define ASH_HOME_SCREEN_WINDOW_SCALE_ANIMATION_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"

namespace gfx {
class Transform;
}

namespace ash {

enum class BackdropWindowMode;

// The class the does the dragged window scale-down animation to home screen or
// scale-up to restore to its original bounds after drag ends. The window will
// be minimized after animation complete if we're heading to the home screen.
class WindowScaleAnimation : public ui::ImplicitAnimationObserver,
                             public aura::WindowObserver {
 public:
  enum class WindowScaleType {
    kScaleDownToHomeScreen,
    kScaleUpToRestore,
  };

  WindowScaleAnimation(
      aura::Window* window,
      WindowScaleType scale_type,
      base::Optional<BackdropWindowMode> original_backdrop_mode,
      base::OnceClosure opt_callback);
  ~WindowScaleAnimation() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // Returns the transform that should be applied to the dragged window if we
  // should head to homescreen after dragging.
  gfx::Transform GetWindowTransformToHomeScreen();

  aura::Window* window_;
  base::Optional<BackdropWindowMode> original_backdrop_mode_;
  base::OnceClosure opt_callback_;

  const WindowScaleType scale_type_;

  ScopedObserver<aura::Window, aura::WindowObserver> window_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(WindowScaleAnimation);
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_WINDOW_SCALE_ANIMATION_H_
