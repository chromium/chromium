// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_DELAYED_ANIMATION_OBSERVER_IMPL_H_
#define ASH_WM_OVERVIEW_DELAYED_ANIMATION_OBSERVER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/delayed_animation_observer.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ash {
class OverviewDelegate;

// An observer that does not watch any animation, but instead has a timeout
// before telling its owner to destroy it. It is used when entering overview
// without any animations but we still want to delay some tasks.
class ASH_EXPORT ForceDelayObserver : public DelayedAnimationObserver {
 public:
  explicit ForceDelayObserver(base::TimeDelta delay);
  ~ForceDelayObserver() override;

  // DelayedAnimationObserver:
  void SetOwner(OverviewDelegate* owner) override;
  void Shutdown() override;

 private:
  void Finish();

  OverviewDelegate* owner_ = nullptr;
  base::WeakPtrFactory<ForceDelayObserver> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ForceDelayObserver);
};

// An observer which watches a overview enter animation and signals its owner
// when the animation it is watching finishes.
class ASH_EXPORT EnterAnimationObserver : public ui::ImplicitAnimationObserver,
                                          public DelayedAnimationObserver {
 public:
  EnterAnimationObserver();
  ~EnterAnimationObserver() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // DelayedAnimationObserver:
  void SetOwner(OverviewDelegate* owner) override;
  void Shutdown() override;

 private:
  OverviewDelegate* owner_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(EnterAnimationObserver);
};

// An observer which watches a overview exit animation and signals its owner
// when the animation it is watching finishes.
class ASH_EXPORT ExitAnimationObserver : public ui::ImplicitAnimationObserver,
                                         public DelayedAnimationObserver {
 public:
  ExitAnimationObserver();
  ~ExitAnimationObserver() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // DelayedAnimationObserver:
  void SetOwner(OverviewDelegate* owner) override;
  void Shutdown() override;

 private:
  OverviewDelegate* owner_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExitAnimationObserver);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_DELAYED_ANIMATION_OBSERVER_IMPL_H_
