// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_MOCK_DRAG_DROP_OBSERVER_H_
#define ASH_DRAG_DROP_MOCK_DRAG_DROP_OBSERVER_H_

#include "base/scoped_observation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/drag_drop_client_observer.h"

namespace aura::client {
class DragDropClient;
}  // namespace aura::client

namespace ui {
namespace mojom {
enum class DragOperation;
}  // namespace mojom

class DropTargetEvent;
}  // namespace ui

namespace ash {

// A mock observer on drag-and-drop events. Used for testing only.
class MockDragDropObserver : public aura::client::DragDropClientObserver {
 public:
  explicit MockDragDropObserver(aura::client::DragDropClient* client);
  MockDragDropObserver(const MockDragDropObserver&) = delete;
  MockDragDropObserver& operator=(const MockDragDropObserver&) = delete;
  ~MockDragDropObserver() override;

  // Stops observing the DragDropClient.
  void ResetObservation();

  // aura::client::DragDropClientObserver:
  MOCK_METHOD(void, OnDragStarted, (), (override));
  MOCK_METHOD(void,
              OnDragUpdated,
              (const ui::DropTargetEvent& event),
              (override));
  MOCK_METHOD(void,
              OnDragCompleted,
              (const ui::DropTargetEvent& event),
              (override));
  MOCK_METHOD(void, OnDragCancelled, (), (override));
  MOCK_METHOD(void, OnDropCompleted, (ui::mojom::DragOperation), (override));
  MOCK_METHOD(void, OnDragDropClientDestroying, (), (override));

 private:
  base::ScopedObservation<aura::client::DragDropClient,
                          aura::client::DragDropClientObserver>
      observation_{this};
};

}  // namespace ash

#endif  // ASH_DRAG_DROP_MOCK_DRAG_DROP_OBSERVER_H_
