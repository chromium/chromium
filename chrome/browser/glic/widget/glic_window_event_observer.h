// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_EVENT_OBSERVER_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_EVENT_OBSERVER_H_

#include <optional>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace gfx {
class Vector2d;
}  // namespace gfx

namespace glic {

class GlicWidget;
class GlicWindowAnimator;
class GlicInstanceCoordinatorBrowserTest;

// Observes mouse and touch events on the provided Glic widget to handle
// dragging.
class GlicWindowEventObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual GlicWindowAnimator* window_animator() = 0;
    virtual void OnDragComplete() = 0;
  };

  GlicWindowEventObserver(base::WeakPtr<GlicWidget> glic_widget,
                          Delegate* delegate);
  ~GlicWindowEventObserver();

  void SetDraggingAreasAndWatchForMouseEvents();
  void AdjustPositionIfNeeded();

  bool IsDragging() { return in_move_loop_; }

  base::WeakPtr<GlicWindowEventObserver> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  FRIEND_TEST_ALL_PREFIXES(GlicInstanceCoordinatorBrowserTest,
                           WidgetClosedDuringDragDoesNotCrash);

  // This is a blocking call that will spin a nested message loop.
  void HandleWindowDragWithOffset(const gfx::Vector2d& mouse_offset);

 private:
  class WindowEventObserverImpl;

  void OnMoveLoopFinished();

  // The widget that this animator is responsible for.
  base::WeakPtr<GlicWidget> widget_;
  const raw_ptr<Delegate> delegate_;
  std::unique_ptr<WindowEventObserverImpl> window_event_observer_impl_;
  bool in_move_loop_ = false;

  base::WeakPtrFactory<GlicWindowEventObserver> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_EVENT_OBSERVER_H_
