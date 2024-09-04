// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/mock_drag_drop_observer.h"

#include "ui/aura/client/drag_drop_client.h"

namespace ash {

MockDragDropObserver::MockDragDropObserver(
    aura::client::DragDropClient* client) {
  observation_.Observe(client);
}

MockDragDropObserver::~MockDragDropObserver() = default;

void MockDragDropObserver::ResetObservation() {
  observation_.Reset();
}

}  // namespace ash
