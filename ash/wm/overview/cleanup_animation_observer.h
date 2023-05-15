// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_CLEANUP_ANIMATION_OBSERVER_H_
#define ASH_WM_OVERVIEW_CLEANUP_ANIMATION_OBSERVER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/overview/delayed_animation_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer_animation_observer.h"

namespace views {
class Widget;
}

namespace ash {

// An observer which holds onto the passed widget until the animation is
// complete.
class ASH_EXPORT CleanupAnimationObserver
    : public ui::ImplicitAnimationObserver,
      public DelayedAnimationObserver {
 public:
  explicit CleanupAnimationObserver(std::unique_ptr<views::Widget> widget);

  CleanupAnimationObserver(const CleanupAnimationObserver&) = delete;
  CleanupAnimationObserver& operator=(const CleanupAnimationObserver&) = delete;

  ~CleanupAnimationObserver() override;

  // ui::ImplicitAnimationObserver:
  // TODO(varkha): Look into all cases when animations are not started such as
  // zero-duration animations and ensure that the object lifetime is handled.
  void OnImplicitAnimationsCompleted() override;

  // DelayedAnimationObserver:
  void SetOwner(OverviewDelegate* owner) override;
  void Shutdown() override;

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<OverviewDelegate, ExperimentalAsh> owner_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_CLEANUP_ANIMATION_OBSERVER_H_
