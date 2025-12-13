// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/watermark_view.h"

#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
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
    WatermarkView view;
    view.SetString("foo", SK_ColorBLACK, SK_ColorWHITE,
                   enterprise_connectors::kWatermarkStyleFontSizeDefault);
    view.GetViewAccessibility().GetAccessibleNodeData(&node_data);
    ASSERT_TRUE(node_data.HasState(ax::mojom::State::kInvisible));
  }
}

class MockWatermarkView : public WatermarkView {
 public:
  MOCK_METHOD(void, InvalidateView, (), (override));
};

TEST(WatermarkViewTest, UnchangedWatermarkSkipsRepaint) {
  std::string watermark_text = "sample_text";
  SkColor fill_color = SK_ColorWHITE;
  SkColor outline_color = SK_ColorBLACK;
  int font_size = 10;

  auto view = MockWatermarkView();
  EXPECT_CALL(view, InvalidateView()).Times(1);
  view.SetString(watermark_text, fill_color, outline_color, font_size);
  view.SetString(watermark_text, fill_color, outline_color, font_size);
}

TEST(WatermarkViewTest, EmptyStringWithStyleChangeDoesNotInvalidateView) {
  std::string watermark_text = "sample_text";
  SkColor old_color = SK_ColorWHITE;
  SkColor new_color = SK_ColorBLACK;
  int font_size = 10;

  auto view = MockWatermarkView();
  EXPECT_CALL(view, InvalidateView()).Times(2);

  // Set to an initial non-empty value, will invalidate.
  view.SetString(watermark_text, old_color, old_color, font_size);

  // Clear twice with different styles. Will invalidate the first time only.
  view.SetString("", old_color, old_color, font_size);
  view.SetString("", new_color, new_color, font_size);
}

}  // namespace enterprise_watermark
