// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_START_ANIMATION_OBSERVER_H_
#define ASH_WM_OVERVIEW_START_ANIMATION_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ash {

// An observer which watches a overview start animation and signals its owner
// when the animation it is watching finishes.
class ASH_EXPORT StartAnimationObserver : public ui::ImplicitAnimationObserver,
                                          public DelayedAnimationObserver {
 public:
  StartAnimationObserver();
  ~StartAnimationObserver() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // DelayedAnimationObserver:
  void SetOwner(WindowSelectorDelegate* owner) override;
  void Shutdown() override;

 private:
  WindowSelectorDelegate* owner_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(StartAnimationObserver);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_START_ANIMATION_OBSERVER_H_