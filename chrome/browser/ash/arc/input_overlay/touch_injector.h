// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_

#include <memory>
#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "ui/events/event_rewriter.h"
#include "ui/gfx/geometry/rect_f.h"

namespace aura {
class Window;
}  // namespace aura

namespace arc {
namespace input_overlay {
// If the following touch move sent immediately, the touch move event is not
// processed correctly by apps. This is a delayed time to send touch move
// event.
constexpr base::TimeDelta kSendTouchMoveDelay = base::Milliseconds(50);

gfx::RectF CalculateWindowContentBounds(aura::Window* window);

}  // namespace input_overlay

// TouchInjector includes all the touch actions related to the specific window
// and performs as a bridge between the ArcInputOverlayManager and the touch
// actions. It implements EventRewriter to transform input events to touch
// events.
class TouchInjector : public ui::EventRewriter {
 public:
  explicit TouchInjector(aura::Window* top_level_window);
  TouchInjector(const TouchInjector&) = delete;
  TouchInjector& operator=(const TouchInjector&) = delete;
  ~TouchInjector() override;

  aura::Window* target_window() { return target_window_; }
  const std::vector<std::unique_ptr<input_overlay::Action>>& actions() const {
    return actions_;
  }

  // Parse Json to actions.
  // Json value format:
  // {
  //   "tap": {
  //     "keyboard": [],
  //     "mouse": []
  //   },
  //   "move": {
  //     "keyboard": [],
  //     "mouse": []
  //   },
  //   ...
  // }
  void ParseActions(const base::Value& root);
  // Notify the EventRewriter whether the text input is focused or not.
  void NotifyTextInputState(bool active);
  // Register the EventRewriter.
  void RegisterEventRewriter();
  // Unregister the EventRewriter.
  void UnRegisterEventRewriter();

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  // If the window is destroying or focusing out, releasing the active touch
  // event.
  void DispatchTouchCancelEvent();

  void SendTouchMoveEvent(const ui::EventRewriter::Continuation,
                          const ui::TouchEvent& event);

  aura::Window* target_window_;
  base::WeakPtr<ui::EventRewriterContinuation> continuation_;
  std::vector<std::unique_ptr<input_overlay::Action>> actions_;
  base::ScopedObservation<ui::EventSource,
                          ui::EventRewriter,
                          &ui::EventSource::AddEventRewriter,
                          &ui::EventSource::RemoveEventRewriter>
      observation_{this};
  bool text_input_active_ = false;

  base::WeakPtrFactory<TouchInjector> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_
