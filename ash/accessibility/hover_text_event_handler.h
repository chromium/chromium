// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_HOVER_TEXT_EVENT_HANDLER_H_
#define ASH_ACCESSIBILITY_HOVER_TEXT_EVENT_HANDLER_H_

#include <cstdint>

#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/pointer_details.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

class HoverTextEventHandlerDelegate;

// Intercepts events when HoverText is enabled.
// Forwards screen coordinates and requests to start/stop hover
// interactions to `AccessibilityController`.
// A touch sequence that starts while Hover Text is active stops raw touch
// propagation and forces gesture recognition, so gestures are synthesized
// without sending raw touch events to underlying windows. Those gestures are
// forwarded as hover points and consumed, so touch exploration does not induce
// drags or clicks. Activating hover text cancels any in-progress touch
// sequences. Only one finger at a time drives hover points while more fingers
// pressed at the same time are consumed without dispatching. Fingers still
// pressed while active that are still pressed when hover text deactivates are
// consumed without going to the rest of the system. Hover text gets
// events after the magnifiers, so magnification touch interaction is not
// impacted. On Search key release, the key-up event is cancelled only if a
// Hover Text bubble was displayed during that Search hold (preventing the
// launcher from opening after hovering while allowing a bare Search tap to
// open the launcher).
//
// TODO(b/568813139): Show a toast on repeated touch tap that hover text state
// is active.
class ASH_EXPORT HoverTextEventHandler : public ui::EventHandler {
 public:
  explicit HoverTextEventHandler(HoverTextEventHandlerDelegate* delegate);
  HoverTextEventHandler(const HoverTextEventHandler&) = delete;
  HoverTextEventHandler& operator=(const HoverTextEventHandler&) = delete;
  ~HoverTextEventHandler() override;

  // Returns true while Hover Text is active, either because the Search
  // activation key is held or because the tray button turned it on.
  bool IsActive() const;
  bool activation_key_held_for_testing() const { return activation_key_held_; }
  bool is_tray_toggled_active_for_testing() const {
    return tray_toggled_active_;
  }

  // Called when Hover Text is activated or deactivated via the shelf status
  // area tray button.
  void OnTrayToggled();

  // Records that a Hover Text bubble was displayed while the Search activation
  // key is held down, so the subsequent Search key-release event is consumed
  // instead of opening the launcher.
  void OnHoverTextBubbleShown();

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  void OnHoverTextDeactivated();
  void HandleSearchKeyEvent(ui::KeyEvent* event);
  void HandleControlKeyEvent(const ui::KeyEvent& event);

  // Called when a key other than Search or Ctrl is pressed, or a mouse
  // press or drag occurs, while Search is held. Search is then the modifier
  // of a keyboard shortcut chord (such as Search+D or Search+Shift+H) or a
  // mouse gesture rather than held to use Hover Text, so this cancels the
  // Search-hold activation for the rest of this press. It reports
  // Hover Text as inactive (unless the tray button keeps it active) through
  // `OnHoverTextKeyEvent()`, so the extension can hide any bubble already shown
  // and stop speech. No separate speech stop is requested (`stop_speech` is
  // false), and the later Search release is not consumed. Search works for
  // Hover Text again once it is released and pressed again.
  void CancelActivationChord();

  void SetActivationKeyHeld(bool is_key_held, bool stop_speech);
  void MaybeDispatchMousePoint(const ui::MouseEvent& event);

  // Sends mouse and touch moves to the extension at most once per dispatch
  // interval. A point that arrives sooner replaces any point already waiting,
  // and is sent one interval after the last point that arrived too soon,
  // unless a later point is sent first. A touch release sends a waiting
  // touch move early, and turning Hover Text off drops the waiting point.
  void ScheduleDispatchPoint(const gfx::Point& screen_point,
                             HoverTextPointerType pointer_type);

  // Sends a point to the extension now and drops any point still waiting.
  void DispatchPoint(const gfx::Point& screen_point,
                     HoverTextPointerType pointer_type);

  // Sends `pending_point_` now if Hover Text is still active. Runs from the
  // timer, and on touch release to send a touch move still waiting.
  void DispatchPendingPoint();

  raw_ptr<HoverTextEventHandlerDelegate> delegate_;
  bool activation_key_held_ = false;
  bool tray_toggled_active_ = false;
  // True from `CancelActivationChord()` until Search is released.
  bool chord_cancelled_ = false;
  bool bubble_shown_during_search_hold_ = false;
  base::TimeTicks last_point_dispatch_time_;
  gfx::Point pending_point_;
  HoverTextPointerType pending_point_pointer_type_ =
      HoverTextPointerType::kMouse;
  base::OneShotTimer point_dispatch_timer_;
  ui::PointerId touch_id_ = ui::kPointerIdUnknown;
  uint32_t tracked_touch_unique_event_id_ = 0;
  gfx::Point tracked_touch_location_;
  base::flat_set<ui::PointerId> consumed_touch_ids_;
  bool consuming_touch_sequence_ = false;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_HOVER_TEXT_EVENT_HANDLER_H_
