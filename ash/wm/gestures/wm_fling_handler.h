// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_WM_FLING_HANDLER_H_
#define ASH_WM_GESTURES_WM_FLING_HANDLER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace aura {
class Window;
}

namespace ui {
class Compositor;
class FlingCurve;
}  // namespace ui

namespace ash {

// This class is a wrapper around a ui::FlingCurve object. It computes a new
// decayed velocity each time the compositor ticks and notifies its users. Used
// for flinging while scrolling in window manager.
class WmFlingHandler : public ui::CompositorAnimationObserver {
 public:
  // Callback which is run on every animation tick. Returns true if the user
  // wants to continue the fling.
  using StepCallback = base::RepeatingCallback<bool(float)>;

  WmFlingHandler(const gfx::Vector2dF& initial_velocity,
                 const aura::Window* root_window,
                 StepCallback on_step_callback,
                 base::RepeatingClosure on_end_callback);
  WmFlingHandler(const WmFlingHandler&) = delete;
  WmFlingHandler& operator=(const WmFlingHandler&) = delete;
  ~WmFlingHandler() override;

 private:
  // ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  void EndFling();

  // Gesture curve of the current active fling. nullptr while a fling is not
  // active.
  std::unique_ptr<ui::FlingCurve> fling_curve_;

  // Velocity of the fling that will gradually decrease during a fling.
  gfx::Vector2dF fling_velocity_;

  // Cached value of an earlier offset that determines values to scroll through
  // by being compared to an updated offset.
  std::optional<gfx::Vector2dF> fling_last_offset_;

  // The compositor we are observing.
  raw_ptr<ui::Compositor> observed_compositor_ = nullptr;

  StepCallback on_step_callback_;
  base::RepeatingClosure on_end_callback_;

  base::WeakPtrFactory<WmFlingHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_WM_FLING_HANDLER_H_
