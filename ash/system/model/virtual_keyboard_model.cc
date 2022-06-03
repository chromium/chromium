// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/virtual_keyboard_model.h"

namespace ash {

VirtualKeyboardModel::VirtualKeyboardModel() = default;
VirtualKeyboardModel::~VirtualKeyboardModel() = default;

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
}

void VirtualKeyboardModel::OnArcInputMethodBoundsChanged(
    const gfx::Rect& bounds) {
  arc_keyboard_bounds_ = bounds;
  const bool new_visible = !bounds.IsEmpty();
  if (visible_ == new_visible)
    return;
  visible_ = new_visible;
  NotifyChanged();
}

void VirtualKeyboardModel::NotifyChanged() {
  for (auto& observer : observers_)
    observer.OnVirtualKeyboardVisibilityChanged();
}

}  // namespace ash
