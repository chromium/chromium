// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/virtual_keyboard_model.h"

#include "ash/public/cpp/keyboard/keyboard_controller.h"

namespace ash {

VirtualKeyboardModel::VirtualKeyboardModel() {
  KeyboardController::Get()->AddObserver(this);
}

VirtualKeyboardModel::~VirtualKeyboardModel() {
  KeyboardController::Get()->RemoveObserver(this);
}

void VirtualKeyboardModel::AddObserver(
    VirtualKeyboardModel::Observer* observer) {
  observers_.AddObserver(observer);
}

void VirtualKeyboardModel::RemoveObserver(
    VirtualKeyboardModel::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void VirtualKeyboardModel::SetInputMethodBoundsTrackerObserver(
    ArcInputMethodBoundsTracker* input_method_bounds_tracker) {
  DCHECK(input_method_bounds_tracker);
  input_method_bounds_tracker->AddObserver(this);
}

void VirtualKeyboardModel::RemoveInputMethodBoundsTrackerObserver(
    ArcInputMethodBoundsTracker* input_method_bounds_tracker) {
  DCHECK(input_method_bounds_tracker);
  input_method_bounds_tracker->RemoveObserver(this);
  arc_keyboard_bounds_ = gfx::Rect();
  UpdateArcKeyboardVisibility();
}

void VirtualKeyboardModel::OnArcInputMethodBoundsChanged(
    const gfx::Rect& bounds) {
  arc_keyboard_bounds_ = bounds;
  UpdateArcKeyboardVisibility();
}

void VirtualKeyboardModel::OnKeyboardEnabledChanged(bool is_enabled) {
  UpdateArcKeyboardVisibility();
}

void VirtualKeyboardModel::UpdateArcKeyboardVisibility() {
  // NOTE: KeyboardController reporting that keyboard is enabled implies that
  // the user is using Chrome keyboard, and thus ARC IME keyboard is not
  // actually being shown (even if ARC input method surface tracker reports
  // non-empyt bounds).
  bool new_visibility = !arc_keyboard_bounds_.IsEmpty() &&
                        !KeyboardController::Get()->IsKeyboardEnabled();
  if (arc_keyboard_visible_ == new_visibility)
    return;
  arc_keyboard_visible_ = new_visibility;

  for (auto& observer : observers_)
    observer.OnVirtualKeyboardVisibilityChanged();
}

}  // namespace ash
