// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_DELEGATE_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"

namespace ash {

class WindowSelectorDelegate;

class ASH_EXPORT DelayedAnimationObserver {
 public:
  virtual ~DelayedAnimationObserver() {}

  // Sets an |owner| that can be notified when the animation that |this|
  // observes completes.
  virtual void SetOwner(WindowSelectorDelegate* owner) = 0;

  // Can be called by the |owner| to delete the owned widget. The |owner| is
  // then responsible for deleting |this| instance of the
  // DelayedAnimationObserver.
  virtual void Shutdown() = 0;
};

// Implement this class to handle the selection event from WindowSelector.
class ASH_EXPORT WindowSelectorDelegate {
 public:
  // Invoked if selection is ended.
  virtual void OnSelectionEnded() = 0;

  // Passes ownership of |animation_observer| to |this| delegate.
  virtual void AddDelayedAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) = 0;

  // Finds and erases |animation_observer| from the list deleting the widget
  // owned by the |animation_observer|. This method should be called when a
  // scheduled animation completes. If the animation completion callback is a
  // result of a window getting destroyed then the
  // DelayedAnimationObserver::Shutdown() should be called first before
  // destroying the window.
  virtual void RemoveAndDestroyAnimationObserver(
      DelayedAnimationObserver* animation_observer) = 0;

  // Passes ownership of |animation_observer| to |this| delegate.
  virtual void AddStartAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) = 0;

  // Finds and erases |animation_observer| from the list which tracks the start
  // animations. This method should be called when a scheduled start overview
  // animation completes.
  virtual void RemoveAndDestroyStartAnimationObserver(
      DelayedAnimationObserver* animation_observer) = 0;

 protected:
  virtual ~WindowSelectorDelegate() {}
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_DELEGATE_H_
