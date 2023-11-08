// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/scoped_mock_holding_space_controller_observer.h"

#include "ash/public/cpp/holding_space/holding_space_controller.h"

namespace ash {

ScopedMockHoldingSpaceControllerObserver::
    ScopedMockHoldingSpaceControllerObserver(
        HoldingSpaceController* controller) {
  observation_.Observe(controller);
}

ScopedMockHoldingSpaceControllerObserver::
    ~ScopedMockHoldingSpaceControllerObserver() = default;

}  // namespace ash
