// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/stylus_battery_view.h"

#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

using StylusBatteryViewTest = AshTestBase;

TEST_F(StylusBatteryViewTest, AccessibleProperties) {
  ui::AXNodeData data;
  auto battery_view = std::make_unique<StylusBatteryView>();

  ASSERT_TRUE(battery_view);
  battery_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kLabelText);
}

}  // namespace ash
