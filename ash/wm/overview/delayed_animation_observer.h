// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_DELAYED_ANIMATION_OBSERVER_H_
#define ASH_WM_OVERVIEW_DELAYED_ANIMATION_OBSERVER_H_

namespace ash {
class OverviewDelegate;

class DelayedAnimationObserver {
 public:
  virtual ~DelayedAnimationObserver() {}

  // Sets an |owner| that can be notified when the animation that |this|
  // observes completes.
  virtual void SetOwner(OverviewDelegate* owner) = 0;

  // Can be called by the |owner| to delete the owned widget. The |owner| is
  // then responsible for deleting |this| instance of the
  // DelayedAnimationObserver.
  virtual void Shutdown() = 0;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_DELAYED_ANIMATION_OBSERVER_H_
