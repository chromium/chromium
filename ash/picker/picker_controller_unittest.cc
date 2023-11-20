// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_controller.h"

#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace {

using PickerControllerTest = AshTestBase;

TEST_F(PickerControllerTest, ToggleWidgetShowsWidgetIfClosed) {
  TestAshWebViewFactory web_view_factory;
  PickerController controller;

  controller.ToggleWidget();

  EXPECT_TRUE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, ToggleWidgetClosesWidgetIfOpen) {
  TestAshWebViewFactory web_view_factory;
  PickerController controller;
  controller.ToggleWidget();
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller.widget_for_testing());

  controller.ToggleWidget();

  widget_destroyed_waiter.Wait();
  EXPECT_FALSE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, ToggleWidgetShowsWidgetIfOpenedThenClosed) {
  TestAshWebViewFactory web_view_factory;
  PickerController controller;
  controller.ToggleWidget();
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller.widget_for_testing());
  controller.ToggleWidget();
  widget_destroyed_waiter.Wait();

  controller.ToggleWidget();

  EXPECT_TRUE(controller.widget_for_testing());
}

}  // namespace
}  // namespace ash
