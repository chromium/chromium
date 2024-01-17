// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_MOCK_HOLDING_SPACE_MODEL_OBSERVER_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_MOCK_HOLDING_SPACE_MODEL_OBSERVER_H_

#include <vector>

#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// A mock implementation of `HoldingSpaceModelObserver` for use in testing.
class MockHoldingSpaceModelObserver : public HoldingSpaceModelObserver {
 public:
  MockHoldingSpaceModelObserver();
  MockHoldingSpaceModelObserver(const MockHoldingSpaceModelObserver&) = delete;
  MockHoldingSpaceModelObserver& operator=(
      const MockHoldingSpaceModelObserver&) = delete;
  ~MockHoldingSpaceModelObserver() override;

  // HoldingSpaceModelObserver:
  MOCK_METHOD(void,
              OnHoldingSpaceItemsAdded,
              (const std::vector<const HoldingSpaceItem*>& items),
              (override));
  MOCK_METHOD(void,
              OnHoldingSpaceItemsRemoved,
              (const std::vector<const HoldingSpaceItem*>& items),
              (override));
  MOCK_METHOD(void,
              OnHoldingSpaceItemUpdated,
              (const HoldingSpaceItem* item,
               const HoldingSpaceItemUpdatedFields& updated_fields),
              (override));
  MOCK_METHOD(void,
              OnHoldingSpaceItemInitialized,
              (const HoldingSpaceItem* item),
              (override));
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_MOCK_HOLDING_SPACE_MODEL_OBSERVER_H_
