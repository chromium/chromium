// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_alignment_indicator.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/display/manager/display_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {

class DisplayAlignmentIndicatorTest : public AshTestBase {
 public:
  DisplayAlignmentIndicatorTest() = default;
  ~DisplayAlignmentIndicatorTest() override = default;

 protected:
  bool DoesPillExist(const DisplayAlignmentIndicator& indicator) {
    return indicator.pill_widget_ != nullptr;
  }

  bool IsPillVisible(const DisplayAlignmentIndicator& indicator) {
    return indicator.pill_widget_ && indicator.pill_widget_->IsVisible();
  }

  bool IsHighlightVisible(const DisplayAlignmentIndicator& indicator) {
    return indicator.indicator_widget_.IsVisible();
  }
};

TEST_F(DisplayAlignmentIndicatorTest, IndicatorWithPill) {
  UpdateDisplay("1920x1080,1366x768");

  const auto& primary_display = display_manager()->GetDisplayAt(0);
  const auto& secondary_display = display_manager()->GetDisplayAt(1);

  gfx::Rect primary_edge;
  gfx::Rect secondary_edge;

  ASSERT_TRUE(display::ComputeBoundary(primary_display, secondary_display,
                                       &primary_edge, &secondary_edge));

  auto indicator = DisplayAlignmentIndicator::CreateWithPill(
      primary_display, primary_edge, "test");

  ASSERT_TRUE(DoesPillExist(*indicator));
  EXPECT_TRUE(IsPillVisible(*indicator));
  EXPECT_TRUE(IsHighlightVisible(*indicator));

  indicator->Hide();

  EXPECT_FALSE(IsPillVisible(*indicator));
  EXPECT_FALSE(IsHighlightVisible(*indicator));

  indicator->Show();

  EXPECT_TRUE(IsPillVisible(*indicator));
  EXPECT_TRUE(IsHighlightVisible(*indicator));
}

TEST_F(DisplayAlignmentIndicatorTest, IndicatorWithoutPill) {
  UpdateDisplay("1920x1080,1366x768");

  const auto& primary_display = display_manager()->GetDisplayAt(0);
  const auto& secondary_display = display_manager()->GetDisplayAt(1);

  gfx::Rect primary_edge;
  gfx::Rect secondary_edge;

  ASSERT_TRUE(display::ComputeBoundary(primary_display, secondary_display,
                                       &primary_edge, &secondary_edge));

  auto indicator =
      DisplayAlignmentIndicator::Create(primary_display, primary_edge);

  ASSERT_FALSE(DoesPillExist(*indicator));
  EXPECT_TRUE(IsHighlightVisible(*indicator));

  indicator->Hide();

  EXPECT_FALSE(IsHighlightVisible(*indicator));

  indicator->Show();

  EXPECT_TRUE(IsHighlightVisible(*indicator));
}

}  // namespace ash
