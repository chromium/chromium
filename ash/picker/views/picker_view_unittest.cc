// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_view.h"

#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

using ::testing::ElementsAre;
using ::testing::Truly;

using PickerViewTest = AshTestBase;

PickerView* GetPickerViewFromWidget(views::Widget& widget) {
  return views::AsViewClass<PickerView>(
      widget.non_client_view()->client_view()->children().front());
}

TEST_F(PickerViewTest, CreateWidgetHasCorrectHierarchy) {
  auto widget = PickerView::CreateWidget();

  // Widget should contain a NonClientView, which has a NonClientFrameView for
  // borders and shadows, and a ClientView with a sole child of the PickerView.
  ASSERT_TRUE(widget);
  ASSERT_TRUE(widget->non_client_view());
  ASSERT_TRUE(widget->non_client_view()->frame_view());
  ASSERT_TRUE(widget->non_client_view()->client_view());
  EXPECT_THAT(widget->non_client_view()->client_view()->children(),
              ElementsAre(Truly(views::IsViewClass<PickerView>)));
}

TEST_F(PickerViewTest, CreateWidgetHasCorrectBorder) {
  auto widget = PickerView::CreateWidget();

  EXPECT_TRUE(widget->non_client_view()->frame_view()->GetBorder());
}

TEST_F(PickerViewTest, BackgroundIsCorrect) {
  auto widget = PickerView::CreateWidget();
  PickerView* view = GetPickerViewFromWidget(*widget);

  ASSERT_TRUE(view);
  ASSERT_TRUE(view->background());
  EXPECT_EQ(
      view->background()->get_color(),
      view->GetColorProvider()->GetColor(cros_tokens::kCrosSysBaseElevated));
}

TEST_F(PickerViewTest, SizeIsCorrect) {
  auto widget = PickerView::CreateWidget();
  widget->Show();
  PickerView* view = GetPickerViewFromWidget(*widget);

  EXPECT_EQ(view->size(), gfx::Size(420, 480));
}

}  // namespace
}  // namespace ash
