// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/pods_overflow_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

namespace {

class PodsOverflowTrayTest : public AshTestBase {
 public:
  PodsOverflowTrayTest() = default;

  PodsOverflowTrayTest(const PodsOverflowTrayTest&) = delete;
  PodsOverflowTrayTest& operator=(const PodsOverflowTrayTest&) = delete;

  ~PodsOverflowTrayTest() override = default;

  // AshtestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kScalableShelfPods);
    AshTestBase::SetUp();
  }

  PodsOverflowTray* GetTray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->pods_overflow_tray();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Tests that the accessible name is set correctly in the accessibility cache.
TEST_F(PodsOverflowTrayTest, AccessibleName) {
  ui::AXNodeData tray_data;
  GetTray()->GetViewAccessibility().GetAccessibleNodeData(&tray_data);
  EXPECT_EQ(tray_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Pods overflow tray");
}

}  // namespace ash
