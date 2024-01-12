// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_zero_state_view.h"

#include <memory>

#include "ash/picker/model/picker_category.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::IsEmpty;
using ::testing::Not;

using PickerZeroStateViewTest = AshTestBase;

TEST_F(PickerZeroStateViewTest, CreatesCategorySections) {
  PickerZeroStateView view(base::DoNothing());

  EXPECT_THAT(view.section_views_for_testing(), Not(IsEmpty()));
}

TEST_F(PickerZeroStateViewTest, LeftClickSelectsCategory) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  base::test::TestFuture<PickerCategory> future;
  auto* view = widget->SetContentsView(
      std::make_unique<PickerZeroStateView>(future.GetRepeatingCallback()));
  ASSERT_THAT(view->section_views_for_testing(), Not(IsEmpty()));
  ASSERT_THAT(view->section_views_for_testing()[0]->item_views_for_testing(),
              Not(IsEmpty()));

  PickerItemView* category_view =
      view->section_views_for_testing()[0]->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(category_view);
  LeftClickOn(category_view);

  EXPECT_EQ(future.Get(), PickerCategory::kEmojis);
}

}  // namespace
}  // namespace ash
