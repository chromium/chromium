// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_DELEGATE_H_
#define ASH_WM_OVERVIEW_OVERVIEW_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"

namespace ash {
class DelayedAnimationObserver;

// Implement this class to handle adding and removing animation observers.
class ASH_EXPORT OverviewDelegate {
 public:
  // Passes ownership of |animation_observer| to |this| delegate.
  virtual void AddExitAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) = 0;

  // Finds and erases |animation_observer| from the list deleting the widget
  // owned by the |animation_observer|. This method should be called when a
  // scheduled animation completes. If the animation completion callback is a
  // result of a window getting destroyed then the
  // DelayedAnimationObserver::Shutdown() should be called first before
  // destroying the window.
  virtual void RemoveAndDestroyExitAnimationObserver(
      DelayedAnimationObserver* animation_observer) = 0;

  // Passes ownership of |animation_observer| to |this| delegate.
  virtual void AddEnterAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) = 0;

  // Finds and erases |animation_observer| from the list which tracks the start
  // animations. This method should be called when a scheduled start overview
  // animation completes.
  virtual void RemoveAndDestroyEnterAnimationObserver(
      DelayedAnimationObserver* animation_observer) = 0;

 protected:
  virtual ~OverviewDelegate() {}
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_DELEGATE_H_
