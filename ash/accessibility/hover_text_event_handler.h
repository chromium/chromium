// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_HOVER_TEXT_EVENT_HANDLER_H_
#define ASH_ACCESSIBILITY_HOVER_TEXT_EVENT_HANDLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"

namespace ash {

class HoverTextEventHandlerDelegate;

// Intercepts events when HoverText is enabled.
// Forwards screen coordinates and requests to start/stop hover
// interactions to `AccessibilityController`.
// A touch sequence that starts while Hover Text is active stops raw touch
// propagation and forces gesture recognition, so gestures are synthesized
// without sending raw touch events to underlying windows. Those gestures are
// forwarded as hover points and consumed, so touch exploration does not induce
// drags or clicks. On Search key release, the key-up event is cancelled only if
// a Hover Text bubble was displayed during that Search hold (preventing the
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
  void HandleSearchKeyEvent(ui::KeyEvent* event);
  void HandleControlKeyEvent(const ui::KeyEvent& event);
  void CancelActivationChord();
  void SetActivationKeyHeld(bool is_key_held, bool stop_speech);

  raw_ptr<HoverTextEventHandlerDelegate> delegate_;

  bool activation_key_held_ = false;
  bool tray_toggled_active_ = false;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_HOVER_TEXT_EVENT_HANDLER_H_
