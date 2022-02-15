// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_

#include <memory>
#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_mode.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "ui/events/event_rewriter.h"
#include "ui/gfx/geometry/rect_f.h"

namespace aura {
class Window;
}  // namespace aura

namespace arc {
namespace input_overlay {
class DisplayOverlayController;

// If the following touch move sent immediately, the touch move event is not
// processed correctly by apps. This is a delayed time to send touch move
// event.
constexpr base::TimeDelta kSendTouchMoveDelay = base::Milliseconds(50);

gfx::RectF CalculateWindowContentBounds(aura::Window* window);

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
  bool is_mouse_locked() const { return is_mouse_locked_; }
  void set_display_mode(DisplayMode mode) { display_mode_ = mode; }
  void set_display_overlay_controller(DisplayOverlayController* controller) {
    display_overlay_controller_ = controller;
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
  class KeyCommand;

  // If the window is destroying or focusing out, releasing the active touch
  // event.
  void DispatchTouchCancelEvent();
  void SendExtraEvent(const ui::EventRewriter::Continuation continuation,
                      const ui::Event& event);
  void DispatchTouchReleaseEventOnMouseUnLock();
  void DispatchTouchReleaseEvent();
  // Json format:
  // "mouse_lock": {
  //   "key": "KeyA",
  //   "modifier": [""]
  // }
  void ParseMouseLock(const base::Value& value);

  void FlipMouseLockFlag();
  // Check if the event located on menu icon.
  bool MenuAnchorPressed(const ui::Event& event,
                         const gfx::RectF& content_bounds);

  aura::Window* target_window_;
  base::WeakPtr<ui::EventRewriterContinuation> continuation_;
  std::vector<std::unique_ptr<Action>> actions_;
  base::ScopedObservation<ui::EventSource,
                          ui::EventRewriter,
                          &ui::EventSource::AddEventRewriter,
                          &ui::EventSource::RemoveEventRewriter>
      observation_{this};
  std::unique_ptr<KeyCommand> mouse_lock_;
  // It is used temporarily for switching view and edit mode.
  // TODO(cuicuiruan): Remove this after the entry point is ready.
  std::unique_ptr<KeyCommand> switch_mode_;
  bool text_input_active_ = false;
  // The mouse is unlocked by default.
  bool is_mouse_locked_ = false;
  DisplayMode display_mode_ = DisplayMode::kView;
  DisplayOverlayController* display_overlay_controller_ = nullptr;

  base::WeakPtrFactory<TouchInjector> weak_ptr_factory_{this};
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_
