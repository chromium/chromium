// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_

#include <memory>
#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/event_source.h"
#include "ui/gfx/geometry/rect_f.h"

namespace aura {
class Window;
}  // namespace aura

namespace arc {
namespace input_overlay {
class DisplayOverlayController;
class Action;

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
  const std::vector<std::unique_ptr<Action>>& actions() const {
    return actions_;
  }
  bool is_mouse_locked() const { return is_mouse_locked_; }
  bool touch_injector_enable() const { return touch_injector_enable_; }
  bool input_mapping_visible() const { return input_mapping_visible_; }
  void set_display_mode(DisplayMode mode) { display_mode_ = mode; }
  void set_display_overlay_controller(DisplayOverlayController* controller) {
    display_overlay_controller_ = controller;
  }
  void store_touch_injector_enable(bool enable) {
    touch_injector_enable_ = enable;
  }
  void store_input_mapping_visible(bool enable) {
    input_mapping_visible_ = enable;
  }

  // Parse Json to actions.
  // Json value format:
  // {
  //   "tap": [
  //     {},
  //     ...
  //   ],
  //   "move": [
  //     {},
  //     ...
  //   ]
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
  friend class TouchInjectorTest;

  struct TouchPointInfo {
    // ID managed by input overlay.
    int rewritten_touch_id;

    // The latest root location of this given touch event.
    gfx::PointF touch_root_location;
  };

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

  // Takes valid touch events and overrides their ids with an id managed by the
  // TouchIdManager.
  std::unique_ptr<ui::TouchEvent> RewriteOriginalTouch(
      const ui::TouchEvent* touch_event);

  // This method will generate a new touch event with a managed touch id.
  std::unique_ptr<ui::TouchEvent> CreateTouchEvent(
      const ui::TouchEvent* touch_event,
      ui::PointerId original_id,
      int managed_touch_id,
      gfx::PointF root_location_f);

  // For test.
  int GetRewrittenTouchIdForTesting(ui::PointerId original_id);
  gfx::PointF GetRewrittenRootLocationForTesting(ui::PointerId original_id);
  int GetRewrittenTouchInfoSizeForTesting();

  aura::Window* target_window_;
  base::WeakPtr<ui::EventRewriterContinuation> continuation_;
  std::vector<std::unique_ptr<Action>> actions_;
  base::ScopedObservation<ui::EventSource,
                          ui::EventRewriter,
                          &ui::EventSource::AddEventRewriter,
                          &ui::EventSource::RemoveEventRewriter>
      observation_{this};
  std::unique_ptr<KeyCommand> mouse_lock_;
  bool text_input_active_ = false;
  // The mouse is unlocked by default.
  bool is_mouse_locked_ = false;
  DisplayMode display_mode_ = DisplayMode::kView;
  DisplayOverlayController* display_overlay_controller_ = nullptr;
  // Linked to game controller toggle in the menu. Set it enabled by default.
  // This is to save status if display overlay is destroyed during window
  // operations.
  bool touch_injector_enable_ = true;
  // Linked to input mapping toggle in the menu. Set it enabled by default. This
  // is to save status if display overlay is destroyed during window operations.
  bool input_mapping_visible_ = true;

  // Key is the original touch id.
  // Value is a struct containing required info for this touch event.
  base::flat_map<ui::PointerId, TouchPointInfo> rewritten_touch_infos_;

  base::WeakPtrFactory<TouchInjector> weak_ptr_factory_{this};
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_
