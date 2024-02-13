// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_TOUCH_MODE_MOUSE_REWRITER_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_TOUCH_MODE_MOUSE_REWRITER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_rewriter.h"

namespace aura {
class WindowTreeHost;
}  // namespace aura

namespace arc {

// An EventRewriter which rewrites certain mouse/trackpad events that are sent
// to phone-optimized ARC apps. For example, right click will be converted to
// long press, as in many phone-optimized apps it is normal to use long press
// for a secondary action rather than right click.
class TouchModeMouseRewriter : public aura::WindowObserver,
                               public ui::EventRewriter {
 public:
  TouchModeMouseRewriter();
  TouchModeMouseRewriter(const TouchModeMouseRewriter&) = delete;
  TouchModeMouseRewriter& operator=(const TouchModeMouseRewriter&) = delete;
  ~TouchModeMouseRewriter() override;

  // Starts rewriting events sent to |window|.
  void EnableForWindow(aura::Window* window);

  // Stops rewriting events sent to |window|.
  void DisableForWindow(aura::Window* window);

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  void SendReleaseEvent(const ui::MouseEvent& original_event,
                        const Continuation continuation);
  void SendScrollEvent(const ui::MouseWheelEvent& original_event,
                       const Continuation continuation);

  ui::EventDispatchDetails RewriteMouseWheelEvent(
      const ui::MouseWheelEvent& event,
      const Continuation continuation);
  ui::EventDispatchDetails RewriteMouseClickEvent(
      const ui::MouseEvent& event,
      const Continuation continuation);
  bool IsInResizeLockedWindow(const aura::Window* window) const;

  // Used for right click long press.
  bool release_event_scheduled_ = false;
  bool left_pressed_ = false;
  bool discard_next_left_release_ = false;

  // Used for mouse wheel smooth scroll.
  int scroll_y_offset_ = 0;
  int scroll_x_offset_ = 0;
  base::TimeDelta scroll_timeout_;

  std::multiset<aura::WindowTreeHost*> hosts_;
  std::set<raw_ptr<const aura::Window, SetExperimental>> enabled_windows_;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};

  base::WeakPtrFactory<TouchModeMouseRewriter> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_TOUCH_MODE_MOUSE_REWRITER_H_
