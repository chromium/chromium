// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_controller.h"

#include "ash/public/cpp/holding_space/scoped_mock_holding_space_controller_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// HoldingSpaceControllerObserverTest ------------------------------------------

// Base class for tests of the `HoldingSpaceController` that make sure it fires
// all observer methods as appropriate.
// TODO(http://b/260612195): Add all `HoldingSpaceControllerObserver` methods to
// this test suite.
using HoldingSpaceControllerObserverTest = testing::Test;

// Tests -----------------------------------------------------------------------

TEST_F(HoldingSpaceControllerObserverTest, Destruction) {
  auto controller = std::make_unique<HoldingSpaceController>();
  ScopedMockHoldingSpaceControllerObserver observer(controller.get());
  EXPECT_CALL(observer, OnHoldingSpaceControllerDestroying());
  controller.reset();
}

}  // namespace ash
