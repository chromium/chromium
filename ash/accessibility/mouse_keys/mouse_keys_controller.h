// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_CONTROLLER_H_
#define ASH_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_CONTROLLER_H_

#include <array>
#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/timer/timer.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

class DragEventRewriter;
class MouseKeysBubbleController;

// Mouse keys is an accessibility feature that allows you to control your mouse
// cursor with the keyboard.  To do this, MouseKeysController ingests key events
// and generates mouse events.
class ASH_EXPORT MouseKeysController : public ui::EventHandler {
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

  // Pause or unpause mouse keys.
  void Toggle();

  // Returns true if the event should be cancelled.
  bool RewriteEvent(const ui::Event& event);

  void set_enabled(bool enabled);
  bool enabled() { return enabled_; }

  void set_paused(bool paused) { paused_ = paused; }
  bool paused() { return paused_; }

  void set_use_primary_keys(bool use_primary_keys) {
    use_primary_keys_ = use_primary_keys;
  }
  bool use_primary_keys() { return use_primary_keys_; }

  void set_left_handed(bool left_handed) { left_handed_ = left_handed; }
  bool left_handed() { return left_handed_; }

  void set_acceleration(double acceleration) { acceleration_ = acceleration; }
  void SetMaxSpeed(double factor) {
    max_speed_ = factor * kBaseSpeedDIPPerSecond * kUpdateFrequencyInSeconds;
  }

  gfx::Point GetLastMousePositionDips() const {
    return last_mouse_position_dips_;
  }

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
    kKeyDoubleClick,
    kKeyDragStart,
    kKeyDragStop,
    kKeySelectLeftButton,
    kKeySelectRightButton,
    kKeySelectBothButtons,
    kKeySelectNextButton,
    kKeyCount,
  };

  enum MouseButton {
    kLeft,
    kRight,
    kBoth,
  };

  MouseKeysBubbleController* GetMouseKeysBubbleControllerForTest();
  DragEventRewriter* GetDragEventRewriterForTest() const {
    return drag_event_rewriter_.get();
  }

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

  void SendMouseEventToLocation(ui::EventType type,
                                const gfx::Point& location,
                                int flags = 0);
  void MoveMouse(const gfx::Vector2d& move_delta_dip);
  void CenterMouseIfUninitialized();

  // Returns true if the event was consumed.
  bool CheckFlagsAndMaybeSendEvent(const ui::KeyEvent& key_event,
                                   ui::DomCode input,
                                   MouseKey output);
  void PressKey(MouseKey key);
  void RefreshVelocity();
  void ReleaseKey(MouseKey key);
  void ResetMovement();
  void SelectNextButton();
  void UpdateCurrentMouseButton(MouseButton mouse_button);
  void UpdateMouseKeysBubble(bool visible,
                             MouseKeysBubbleIconType icon,
                             const int name_resource_id);
  void UpdateState();
  void EndDragOperation();

  bool enabled_ = false;
  bool paused_ = false;
  bool use_primary_keys_ = false;
  bool left_handed_ = false;
  double acceleration_ = kDefaultAcceleration;
  double max_speed_;
  gfx::Vector2d move_direction_;
  double speed_ = 0;
  MouseButton current_mouse_button_ = kLeft;

  std::array<bool, kKeyCount> pressed_keys_;
  bool dragging_ = false;
  gfx::Point last_mouse_position_dips_ = gfx::Point(-1, -1);
  int event_flags_ = 0;
  base::RepeatingTimer update_timer_;

  // Used to control the MouseKeys bubble UI.
  std::unique_ptr<MouseKeysBubbleController> mouse_keys_bubble_controller_;
  // Adapts drag events for Mouse Keys (e.g. text selection).
  std::unique_ptr<DragEventRewriter> drag_event_rewriter_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_CONTROLLER_H_
