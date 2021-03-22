// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/marker/marker_controller.h"

namespace ash {

namespace {
MarkerController* g_instance = nullptr;
}

MarkerController::MarkerController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

MarkerController::~MarkerController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
MarkerController* MarkerController::Get() {
  return g_instance;
}

void MarkerController::AddObserver(MarkerObserver* observer) {
  observers_.AddObserver(observer);
}

void MarkerController::RemoveObserver(MarkerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MarkerController::SetEnabled(bool enabled) {
  if (enabled == is_enabled())
    return;

  FastInkPointerController::SetEnabled(enabled);
  NotifyStateChanged(enabled);
}

views::View* MarkerController::GetPointerView() const {
  // TODO(https://crbug.com/1187191): Add marker implementation.
  return nullptr;
}

void MarkerController::CreatePointerView(base::TimeDelta presentation_delay,
                                         aura::Window* root_window) {
  // TODO(https://crbug.com/1187191): Add marker implementation.
}

void MarkerController::UpdatePointerView(ui::TouchEvent* event) {
  // TODO(https://crbug.com/1187191): Add marker implementation.
}

void MarkerController::DestroyPointerView() {
  // TODO(https://crbug.com/1187191): Add marker implementation.
}

void MarkerController::NotifyStateChanged(bool enabled) {
  for (MarkerObserver& observer : observers_)
    observer.OnMarkerStateChanged(enabled);
}

}  // namespace ash
