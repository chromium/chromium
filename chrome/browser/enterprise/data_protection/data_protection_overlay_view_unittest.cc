// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_overlay_view.h"

#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace enterprise_data_protection {

TEST(DataProtectionOverlayViewTest, InvisibleToAccessibility) {
  {
    ui::AXNodeData node_data;
    DataProtectionOverlayView().GetViewAccessibility().GetAccessibleNodeData(
        &node_data);
    ASSERT_TRUE(node_data.HasState(ax::mojom::State::kInvisible));
  }
  {
    ui::AXNodeData node_data;
    DataProtectionOverlayView view;
    view.SetWatermarkText(
        "foo", SK_ColorBLACK, SK_ColorWHITE,
        enterprise_connectors::kWatermarkStyleFontSizeDefault);
    view.GetViewAccessibility().GetAccessibleNodeData(&node_data);
    ASSERT_TRUE(node_data.HasState(ax::mojom::State::kInvisible));
  }
}

class MockDataProtectionOverlayView : public DataProtectionOverlayView {
 public:
  MOCK_METHOD(void, InvalidateView, (), (override));
};

TEST(DataProtectionOverlayViewTest, UnchangedWatermarkSkipsRepaint) {
  std::string watermark_text = "sample_text";
  SkColor fill_color = SK_ColorWHITE;
  SkColor outline_color = SK_ColorBLACK;
  int font_size = 10;

  auto view = MockDataProtectionOverlayView();
  EXPECT_CALL(view, InvalidateView()).Times(1);
  view.SetWatermarkText(watermark_text, fill_color, outline_color, font_size);
  view.SetWatermarkText(watermark_text, fill_color, outline_color, font_size);
}

TEST(DataProtectionOverlayViewTest,
     EmptyStringWithStyleChangeDoesNotInvalidateView) {
  std::string watermark_text = "sample_text";
  SkColor old_color = SK_ColorWHITE;
  SkColor new_color = SK_ColorBLACK;
  int font_size = 10;

  auto view = MockDataProtectionOverlayView();
  EXPECT_CALL(view, InvalidateView()).Times(2);

  // Set to an initial non-empty value, will invalidate.
  view.SetWatermarkText(watermark_text, old_color, old_color, font_size);

  // Clear twice with different styles. Will invalidate the first time only.
  view.SetWatermarkText("", old_color, old_color, font_size);
  view.SetWatermarkText("", new_color, new_color, font_size);
}

}  // namespace enterprise_data_protection
