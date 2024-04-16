// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_CONTROLLER_H_
#define ASH_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/base/ime/ash/ime_bridge_observer.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

// Mouse keys is an accessibility feature that allows you to control your mouse
// cursor with the keyboard.  To do this, MouseKeysController ingests key events
// and generates mouse events.
class ASH_EXPORT MouseKeysController : public ui::EventHandler,
                                       public ash::IMEBridgeObserver,
                                       public ui::InputMethodObserver {
 public:
  // TODO(259372916): Find a good base speed/acceleration.
  // Base movement speed in DIP/s.
  static constexpr int kBaseSpeedDIPPerSecond = 80;
  static constexpr int kBaseAccelerationDIPPerSecondSquared = 80;

  // The default acceleration as a scale factor ranging from 0-1 for mouse keys
  // movement.
  static constexpr double kDefaultAcceleration = 0.2;

  // The default max speed as a factor of the minimum speed for mouse keys
  // movement.  Ranges from 0-10.
  static constexpr double kDefaultMaxSpeed = 5;

  // Frequency that the position is updated in Hz.
  static constexpr double kUpdateFrequencyInSeconds = 0.05;

  MouseKeysController();

  MouseKeysController(const MouseKeysController&) = delete;
  MouseKeysController& operator=(const MouseKeysController&) = delete;

  ~MouseKeysController() override;

  // Returns true if the event should be cancelled.
  bool RewriteEvent(const ui::Event& event);

  void set_enabled(bool enabled) { enabled_ = enabled; }
  bool enabled() { return enabled_; }

  void set_left_handed(bool left_handed) { left_handed_ = left_handed; }
  bool left_handed() { return left_handed_; }

  void set_acceleration(double acceleration) { acceleration_ = acceleration; }
  void SetMaxSpeed(double factor) {
    max_speed_ = factor * kBaseSpeedDIPPerSecond * kUpdateFrequencyInSeconds;
  }

  void set_disable_in_text_fields(bool value) {
    disable_in_text_fields_ = value;
  }
  bool disable_in_text_fields() { return disable_in_text_fields_; }

  enum MouseKey {
    kKeyUpLeft = 0,
    kKeyUp,
    kKeyUpRight,
    kKeyLeft,
    kKeyRight,
    kKeyDownLeft,
    kKeyDown,
    kKeyDownRight,
    kKeyClick,
    kKeyCount,
  };

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

  // ash::IMEBridgeObserver:
  void OnInputContextHandlerChanged() override;

  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override;

  void SendMouseEventToLocation(ui::EventType type, const gfx::Point& location);
  void MoveMouse(const gfx::Vector2d& move_delta_dip);
  void CenterMouseIfUninitialized();

  // Returns true if the event was consumed.
  bool CheckFlagsAndMaybeSendEvent(const ui::KeyEvent& key_event,
                                   ui::DomCode input,
                                   MouseKey output);
  void PressKey(MouseKey key);
  void ReleaseKey(MouseKey key);
  void RefreshVelocity();
  void UpdateState();

  bool enabled_ = false;
  bool paused_ = false;
  bool paused_for_text_ = false;
  bool disable_in_text_fields_ = true;
  bool left_handed_ = false;
  double acceleration_ = kDefaultAcceleration;
  double max_speed_;
  gfx::Vector2d move_direction_;
  double speed_ = 0;

  // The currently active input method, observed for focus changes.
  base::ScopedObservation<ui::InputMethod, ui::InputMethodObserver>
      input_method_observer_{this};
  bool pressed_keys_[kKeyCount];
  gfx::Point last_mouse_position_dips_ = gfx::Point(-1, -1);
  int event_flags_ = 0;
  base::RepeatingTimer update_timer_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_CONTROLLER_H_
