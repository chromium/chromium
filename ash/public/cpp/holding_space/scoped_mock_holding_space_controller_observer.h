// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_SCOPED_MOCK_HOLDING_SPACE_CONTROLLER_OBSERVER_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_SCOPED_MOCK_HOLDING_SPACE_CONTROLLER_OBSERVER_H_

#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"

#include "base/scoped_observation.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class HoldingSpaceController;

// An implementation of `HoldingSpaceControllerObserver` that enables its
// methods to be mocked. Observes the given `HoldingSpaceController` with scoped
// semantics.
class ScopedMockHoldingSpaceControllerObserver
    : public HoldingSpaceControllerObserver {
 public:
  explicit ScopedMockHoldingSpaceControllerObserver(
      HoldingSpaceController* controller);
  ~ScopedMockHoldingSpaceControllerObserver() override;

  // HoldingSpaceControllerObserver:
  MOCK_METHOD(void, OnHoldingSpaceControllerDestroying, (), (override));
  MOCK_METHOD(void,
              OnHoldingSpaceTrayBubbleVisibilityChanged,
              (const HoldingSpaceTray*, bool),
              (override));

 private:
  base::ScopedObservation<HoldingSpaceController,
                          HoldingSpaceControllerObserver>
      observation_{this};
};
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_SCOPED_MOCK_HOLDING_SPACE_CONTROLLER_OBSERVER_H_
