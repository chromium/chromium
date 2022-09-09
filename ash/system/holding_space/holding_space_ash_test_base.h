// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ASH_TEST_BASE_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ASH_TEST_BASE_H_

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/mock_holding_space_client.h"
#include "ash/test/ash_test_base.h"

namespace ash {

// Base class for tests of holding space in ash.
class HoldingSpaceAshTestBase : public AshTestBase {
 public:
  HoldingSpaceAshTestBase();
  HoldingSpaceAshTestBase(const HoldingSpaceAshTestBase&) = delete;
  HoldingSpaceAshTestBase& operator=(const HoldingSpaceAshTestBase&) = delete;
  ~HoldingSpaceAshTestBase() override;

  // Adds an item of the specified `type` backed by a file at the specified
  // `file_path` to the model for the currently active user.
  void AddItem(HoldingSpaceItem::Type type, const base::FilePath& file_path);

  // Removes all items from the model for the currently active user.
  void RemoveAllItems();

 protected:
  // AshTestBase:
  void SetUp() override;

 private:
  testing::NiceMock<MockHoldingSpaceClient> client_;
  HoldingSpaceModel model_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ASH_TEST_BASE_H_
