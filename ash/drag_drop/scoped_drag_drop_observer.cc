// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/scoped_drag_drop_observer.h"

#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/scoped_observation.h"

namespace ui {
class DropTargetEvent;
}  // namespace ui

namespace ash {

ScopedDragDropObserver::ScopedDragDropObserver(
    aura::client::DragDropClient* client,
    base::RepeatingCallback<void(const ui::DropTargetEvent*)> event_callback)
    : event_callback_(std::move(event_callback)) {
  drag_drop_client_observer_.Observe(client);
  shell_observer_.Observe(ash::Shell::Get());
}

ScopedDragDropObserver::~ScopedDragDropObserver() = default;

void ScopedDragDropObserver::OnDragUpdated(const ui::DropTargetEvent& event) {
  event_callback_.Run(&event);
}

void ScopedDragDropObserver::OnDragCompleted(const ui::DropTargetEvent& event) {
  event_callback_.Run(/*event=*/nullptr);
}

void ScopedDragDropObserver::OnDragCancelled() {
  event_callback_.Run(/*event=*/nullptr);
}

void ScopedDragDropObserver::OnShellDestroying() {
  drag_drop_client_observer_.Reset();
}

}  // namespace ash
