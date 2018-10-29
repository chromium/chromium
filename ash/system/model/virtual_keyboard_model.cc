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

void VirtualKeyboardModel::SetInputMethodSurfaceManagerObserver(
    ArcInputMethodSurfaceManager* input_method_surface_manager) {
  DCHECK(input_method_surface_manager);
  input_method_surface_manager->AddObserver(this);
}

void VirtualKeyboardModel::RemoveInputMethodSurfaceManagerObserver(
    ArcInputMethodSurfaceManager* input_method_surface_manager) {
  DCHECK(input_method_surface_manager);
  input_method_surface_manager->RemoveObserver(this);
}

void VirtualKeyboardModel::OnArcInputMethodSurfaceBoundsChanged(
    const gfx::Rect& bounds) {
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
