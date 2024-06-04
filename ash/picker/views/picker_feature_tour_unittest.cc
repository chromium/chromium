// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_feature_tour.h"

#include "ash/test/ash_test_base.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using PickerFeatureTourTest = AshTestBase;

TEST_F(PickerFeatureTourTest, ShowShowsDialog) {
  PickerFeatureTour feature_tour;

  feature_tour.Show(base::DoNothing());

  views::Widget* widget = feature_tour.widget_for_testing();
  EXPECT_NE(widget, nullptr);
  views::test::WidgetVisibleWaiter(widget).Wait();
}

TEST_F(PickerFeatureTourTest,
       ClickingCompleteButtonClosesWidgetAndTriggersCallback) {
  PickerFeatureTour feature_tour;
  base::test::TestFuture<void> completed_future;
  feature_tour.Show(completed_future.GetRepeatingCallback());
  views::test::WidgetVisibleWaiter(feature_tour.widget_for_testing()).Wait();

  views::Button* button = feature_tour.complete_button_for_testing();
  ASSERT_NE(button, nullptr);
  ViewDrawnWaiter().Wait(button);
  LeftClickOn(button);

  views::test::WidgetDestroyedWaiter(feature_tour.widget_for_testing()).Wait();
  EXPECT_TRUE(completed_future.Wait());
  EXPECT_EQ(feature_tour.widget_for_testing(), nullptr);
}

}  // namespace
}  // namespace ash
