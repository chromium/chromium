// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/stylus_battery_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

using StylusBatteryViewTest = AshTestBase;

TEST_F(StylusBatteryViewTest, AccessibleProperties) {
  ui::AXNodeData data;
  auto battery_view = std::make_unique<StylusBatteryView>();

  ASSERT_TRUE(battery_view);
  battery_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kLabelText);
  EXPECT_EQ(
      data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      l10n_util::GetStringFUTF16(
          IDS_ASH_STYLUS_BATTERY_PERCENT_ACCESSIBLE,
          base::NumberToString16(
              battery_view->stylus_battery_delegate_.battery_level().value_or(
                  0))));
  PeripheralBatteryListener::BatteryInfo latest_battery;
  latest_battery.level = 50;
  battery_view->stylus_battery_delegate_.OnUpdatedBatteryLevel(latest_battery);

  data = ui::AXNodeData();
  battery_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(
      data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      l10n_util::GetStringFUTF16(
          IDS_ASH_STYLUS_BATTERY_PERCENT_ACCESSIBLE,
          base::NumberToString16(
              battery_view->stylus_battery_delegate_.battery_level().value_or(
                  0))));
}

}  // namespace ash
