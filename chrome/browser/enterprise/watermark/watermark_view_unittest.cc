// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/watermark_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace enterprise_watermark {

TEST(WatermarkViewTest, InvisibleToAccessibility) {
  {
    ui::AXNodeData node_data;
    WatermarkView().GetViewAccessibility().GetAccessibleNodeData(&node_data);
    ASSERT_TRUE(node_data.HasState(ax::mojom::State::kInvisible));
  }
  {
    ui::AXNodeData node_data;
    WatermarkView("foo").GetViewAccessibility().GetAccessibleNodeData(
        &node_data);
    ASSERT_TRUE(node_data.HasState(ax::mojom::State::kInvisible));
  }
}

}  // namespace enterprise_watermark
