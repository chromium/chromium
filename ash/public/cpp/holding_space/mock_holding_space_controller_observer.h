// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_MOCK_HOLDING_SPACE_CONTROLLER_OBSERVER_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_MOCK_HOLDING_SPACE_CONTROLLER_OBSERVER_H_

#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// An implementation of `HoldingSpaceControllerObserver` that enables its
// methods to be mocked.
class MockHoldingSpaceControllerObserver
    : public HoldingSpaceControllerObserver {
 public:
  MockHoldingSpaceControllerObserver();
  ~MockHoldingSpaceControllerObserver() override;

  // HoldingSpaceControllerObserver:
  MOCK_METHOD(void, OnHoldingSpaceControllerDestroying, (), (override));
};
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_MOCK_HOLDING_SPACE_CONTROLLER_OBSERVER_H_
