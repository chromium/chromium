// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_STICKY_KEYS_STICKY_KEYS_CONTROLLER_H_
#define ASH_ACCESSIBILITY_STICKY_KEYS_STICKY_KEYS_CONTROLLER_H_

#include <memory>

#include "ash/accessibility/sticky_keys/sticky_keys_state.h"
#include "ash/ash_export.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"

namespace ui {
class Event;
class KeyEvent;
class MouseEvent;
}  // namespace ui

namespace ash {

class StickyKeysOverlay;
class StickyKeysHandler;

// StickyKeysController is an accessibility feature for users to be able to
// compose key and mouse event with modifier keys without simultaneous key
// press event. Instead they can compose events separately pressing each of the
// modifier keys involved.
// e.g. Composing Ctrl + T
//       User Action   : The KeyEvent widget will receives
// ----------------------------------------------------------
// 1. Press Ctrl key   : Ctrl Keydown.
// 2. Release Ctrl key : No event
// 3. Press T key      : T keydown event with ctrl modifier.
// 4.                  : Ctrl Keyup
// 5. Release T key    : T keyup without ctrl modifier (Windows behavior)
//
// By typing same modifier keys twice, users can generate bunch of modified key
// events.
// e.g. To focus tabs consistently by Ctrl + 1, Ctrl + 2 ...
//       User Action   : The KeyEvent widget will receives
// ----------------------------------------------------------
// 1. Press Ctrl key   : Ctrl Keydown
// 2. Release Ctrl key : No event
// 3. Press Ctrl key   : No event
// 4. Release Ctrl key : No event
// 5. Press 1 key      : 1 Keydown event with Ctrl modifier.
// 6. Release 1 key    : 1 Keyup event with Ctrl modifier.
// 7. Press 2 key      : 2 Keydown event with Ctrl modifier.
// 8. Release 2 key    : 2 Keyup event with Ctrl modifier.
// 9. Press Ctrl key   : No event
// 10. Release Ctrl key: Ctrl Keyup
//
// In the case of Chrome OS, StickyKeysController supports Shift,Alt,Ctrl
// modifiers. Each handling or state is performed independently.
//
// StickyKeysController is disabled by default.
class ASH_EXPORT StickyKeysController : public ui::EventRewriter {
 public:
  StickyKeysController();

  StickyKeysController(const StickyKeysController&) = delete;
  StickyKeysController& operator=(const StickyKeysController&) = delete;

  ~StickyKeysController() override;

  // Activate sticky keys to intercept and modify incoming events.
  void Enable(bool enabled);

  void SetMod3AndAltGrModifiersEnabled(bool mod3_enabled, bool altgr_enabled);

  void SetFnModifierEnabled(bool fn_enabled);

  // Update StickyKeysOverlay bounds (e.g. if the workspace area changed).
  void UpdateStickyKeysOverlayBoundsIfNeeded();

  // Returns the StickyKeyOverlay used by the controller. Ownership is not
  // passed.
  StickyKeysOverlay* GetOverlayForTest();

  // ui::EventRewriter:
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* rewritten_event) override;
  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      std::unique_ptr<ui::Event>* new_event) override;

  bool enabled_for_test() const { return enabled_; }

 private:
  // Rewrite keyboard event.
  ui::EventRewriteStatus RewriteKeyEvent(
      const ui::KeyEvent& event,
      std::unique_ptr<ui::Event>* rewritten_event);

  // Rewrite mouse event.
  ui::EventRewriteStatus RewriteMouseEvent(
      const ui::MouseEvent& event,
      std::unique_ptr<ui::Event>* rewritten_event);

  // Rewrite scroll event.
  ui::EventRewriteStatus RewriteScrollEvent(
      const ui::ScrollEvent& event,
      std::unique_ptr<ui::Event>* rewritten_event);

  // Updates the overlay UI with the current state of the sticky keys.
  void UpdateOverlay();

  // Whether sticky keys is activated and modifying events.
  bool enabled_;

  // Whether the current layout has a mod3 key.
  bool mod3_enabled_;

  // Whether the current layout has an altgr key.
  bool altgr_enabled_;

  // Whether the current internal keyboard has an fn key.
  bool fn_enabled_;

  // Sticky key handlers.
  std::unique_ptr<StickyKeysHandler> shift_sticky_key_;
  std::unique_ptr<StickyKeysHandler> alt_sticky_key_;
  std::unique_ptr<StickyKeysHandler> altgr_sticky_key_;
  std::unique_ptr<StickyKeysHandler> ctrl_sticky_key_;
  std::unique_ptr<StickyKeysHandler> mod3_sticky_key_;
  std::unique_ptr<StickyKeysHandler> search_sticky_key_;
  std::unique_ptr<StickyKeysHandler> fn_sticky_key_;

  std::unique_ptr<StickyKeysOverlay> overlay_;
};

// StickyKeysHandler handles key event and controls sticky keysfor specific
// modifier keys. If monitored keyboard events are received, StickyKeysHandler
// changes internal state. If non modifier keyboard events or mouse events are
// received, StickyKeysHandler will append modifier based on internal state.
// For other events, StickyKeysHandler does nothing.
//
// The DISABLED state is default state and any incoming non modifier keyboard
// events will not be modified. The ENABLED state is one shot modification
// state. Only next keyboard event will be modified. After that, internal state
// will be back to DISABLED state with sending modifier keyup event. In the case
// of LOCKED state, all incomming keyboard events will be modified. The LOCKED
// state will be back to DISABLED state by next monitoring modifier key.
//
// The detailed state flow as follows:
//                                     Current state
//                  |   DISABLED    |    ENABLED     |    LOCKED   |
// ----------------------------------------------------------------|
// Modifier KeyDown |   noop        |    noop(*)     |    noop(*)  |
// Modifier KeyUp   | To ENABLED(*) | To LOCKED(*)   | To DISABLED |
// Normal KeyDown   |   noop        | To DISABLED(#) |    noop(#)  |
// Normal KeyUp     |   noop        |    noop        |    noop(#)  |
// Other KeyUp/Down |   noop        |    noop        |    noop     |
// Mouse Press      |   noop        |    noop(#)     |    noop(#)  |
// Mouse Release    |   noop        | To DISABLED(#) |    noop(#)  |
// Mouse Wheel      |   noop        | To DISABLED(#) |    noop(#)  |
// Other Mouse Event|   noop        |    noop        |    noop     |
//
// Here, (*) means key event will be consumed by StickyKeys, and (#) means event
// is modified.
class ASH_EXPORT StickyKeysHandler {
 public:
  explicit StickyKeysHandler(ui::EventFlags modifier_flag);

  StickyKeysHandler(const StickyKeysHandler&) = delete;
  StickyKeysHandler& operator=(const StickyKeysHandler&) = delete;

  ~StickyKeysHandler();

  // Handles keyboard event. Returns true if Sticky key consumes keyboard event.
  // Sets its own modifier flag in |mod_down_flags| if it is active and needs
  // to be added to the event, and sets |released| if releasing it.
  bool HandleKeyEvent(const ui::KeyEvent& event,
                      int* mod_down_flags,
                      bool* released);

  // Handles mouse event. Returns true if sticky key consumes mouse event.
  // Sets its own modifier flag in |mod_down_flags| if it is active and needs
  // to be added to the event, and sets |released| if releasing it.
  bool HandleMouseEvent(const ui::MouseEvent& event,
                        int* mod_down_flags,
                        bool* released);

  // Handles scroll event. Returns true if sticky key consumes scroll event.
  // Sets its own modifier flag in |mod_down_flags| if it is active and needs
  // to be added to the event, and sets |released| if releasing it.
  bool HandleScrollEvent(const ui::ScrollEvent& event,
                         int* mod_down_flags,
                         bool* released);

  // Fetches a pending modifier-up event if one exists and the return
  // parameter |new_event| is available (i.e. not set). Returns the number
  // of pending events still remaining to be returned.
  int GetModifierUpEvent(std::unique_ptr<ui::Event>* new_event);

  // Returns current internal state.
  StickyKeyState current_state() const { return current_state_; }

  // Informs this StickyKeyHandler if altgr is active. When altgr is active,
  // right-alt will active the altgr key and not the alt key.
  void set_altgr_active(bool altgr_active) { altgr_active_ = altgr_active; }

 private:
  // Represents event type in Sticky Key context.
  enum KeyEventType {
    TARGET_MODIFIER_DOWN,  // The monitoring modifier key is down.
    TARGET_MODIFIER_UP,    // The monitoring modifier key is up.
    NORMAL_KEY_DOWN,       // The non modifier key is down.
    NORMAL_KEY_UP,         // The non modifier key is up.
    OTHER_MODIFIER_DOWN,   // The modifier key but not monitored key is down.
    OTHER_MODIFIER_UP,     // The modifier key but not monitored key is up.
  };

  // Translates event type and key code to sticky keys event type.
  KeyEventType TranslateKeyEvent(ui::EventType type,
                                 ui::KeyboardCode key_code,
                                 ui::DomCode code);

  // Handles key event in DISABLED state. Returns true if sticky keys
  // consumes the keyboard event.
  bool HandleDisabledState(const ui::KeyEvent& event);

  // Handles key event in ENABLED state. Returns true if sticky keys
  // consumes the keyboard event.
  bool HandleEnabledState(const ui::KeyEvent& event,
                          int* mod_down_flags,
                          bool* released);

  // Handles key event in LOCKED state. Returns true if sticky keys
  // consumes the keyboard event.
  bool HandleLockedState(const ui::KeyEvent& event,
                         int* mod_down_flags,
                         bool* released);

  // The modifier flag to be monitored and appended to events.
  const ui::EventFlags modifier_flag_;

  // The current sticky key status.
  StickyKeyState current_state_;

  // True if we received the TARGET_MODIFIER_DOWN event while in the DISABLED
  // state but before we receive the TARGET_MODIFIER_UP event. Normal
  // shortcuts (eg. ctrl + t) during this time will prevent a transition to
  // the ENABLED state.
  bool preparing_to_enable_;

  // Tracks the scroll direction of the current scroll sequence. Sticky keys
  // stops modifying the scroll events of the sequence when the direction
  // changes. If no sequence is tracked, the value is 0.
  int scroll_delta_;

  // The modifier up key event to be sent on non modifier key on ENABLED state.
  std::unique_ptr<ui::KeyEvent> modifier_up_event_;

  // Whether altgr is active for this StickyKeysHandler. This is relevant for
  // alt and altgr StickyKeysHandlers, which must decide whether right-alt
  // is normal alt or altgr depending on keyboard layout.
  bool altgr_active_ = false;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_STICKY_KEYS_STICKY_KEYS_CONTROLLER_H_
