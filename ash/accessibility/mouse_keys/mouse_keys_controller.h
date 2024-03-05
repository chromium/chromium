// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_CONTROLLER_H_
#define ASH_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ui/events/event_handler.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

// Mouse keys is an accessibility feature that allows you to control your mouse
// cursor with the keyboard.  To do this, MouseKeysController ingests key events
// and generates mouse events.
class ASH_EXPORT MouseKeysController : public ui::EventHandler {
 public:
  // TODO(259372916): Add acceleration.
  // TODO(259372916): Find a good base speed.
  static constexpr int kMoveDeltaDIP = 8;

  MouseKeysController();

  MouseKeysController(const MouseKeysController&) = delete;
  MouseKeysController& operator=(const MouseKeysController&) = delete;

  ~MouseKeysController() override;

  // Returns true if the event should be cancelled.
  bool RewriteEvent(const ui::Event& event);

  void SetEnabled(bool enabled);
  bool IsEnabled() const { return enabled_; }

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

  void SendMouseEventToLocation(ui::EventType type, const gfx::Point& location);
  void MoveMouse(float x_direction, float y_direction);
  void CenterMouseIfUninitialized();

  bool enabled_ = false;
  gfx::Point last_mouse_position_dips_ = gfx::Point(-1, -1);
  int event_flags_ = 0;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_CONTROLLER_H_
