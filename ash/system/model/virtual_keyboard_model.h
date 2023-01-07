// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_VIRTUAL_KEYBOARD_MODEL_H_
#define ASH_SYSTEM_MODEL_VIRTUAL_KEYBOARD_MODEL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/arc/arc_input_method_bounds_tracker.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "base/observer_list.h"

namespace ash {

// Model to store virtual keyboard visibility state.
class ASH_EXPORT VirtualKeyboardModel
    : public ArcInputMethodBoundsTracker::Observer,
      public KeyboardControllerObserver {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    virtual void OnVirtualKeyboardVisibilityChanged() = 0;
  };

  VirtualKeyboardModel();

  VirtualKeyboardModel(const VirtualKeyboardModel&) = delete;
  VirtualKeyboardModel& operator=(const VirtualKeyboardModel&) = delete;

  ~VirtualKeyboardModel() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Start/stop observing ArcInputMethodBoundsTracker.
  void SetInputMethodBoundsTrackerObserver(
      ArcInputMethodBoundsTracker* input_method_bounds_tracker);
  void RemoveInputMethodBoundsTrackerObserver(
      ArcInputMethodBoundsTracker* input_method_bounds_tracker);

  // ArcInputMethodBoundsTracker::Observer:
  void OnArcInputMethodBoundsChanged(const gfx::Rect& bounds) override;

  // KeyboardControllerObserver:
  void OnKeyboardEnabledChanged(bool is_enabled) override;

  bool arc_keyboard_visible() const { return arc_keyboard_visible_; }
  const gfx::Rect& arc_keyboard_bounds() const { return arc_keyboard_bounds_; }

 private:
  // Sets `arc_keyboard_visible_` depending on last reported ARC input method
  // bounds and the keyboard controller state. Notifies observes of the
  // visibility change if the `arc_keyboard_visible_` value changed..
  void UpdateArcKeyboardVisibility();

  // Whether ARC IME keyboard is currently visible..
  bool arc_keyboard_visible_ = false;

  gfx::Rect arc_keyboard_bounds_;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_VIRTUAL_KEYBOARD_MODEL_H_
