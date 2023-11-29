// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_controller.h"

#include "ash/public/cpp/picker/picker_client.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace {

using PickerControllerTest = AshTestBase;

// A PickerClient implementation used for testing.
// Automatically sets itself as the client when it's created, and unsets itself
// when it's destroyed.
class TestPickerClient : public PickerClient {
 public:
  explicit TestPickerClient(PickerController* controller)
      : controller_(controller) {
    controller_->SetClient(this);
  }
  ~TestPickerClient() override { controller_->SetClient(nullptr); }

  std::unique_ptr<ash::AshWebView> CreateWebView(
      const ash::AshWebView::InitParams& params) override {
    return web_view_factory_.Create(params);
  }

 private:
  TestAshWebViewFactory web_view_factory_;
  raw_ptr<PickerController> controller_ = nullptr;
};

TEST_F(PickerControllerTest, ToggleWidgetShowsWidgetIfClosed) {
  PickerController controller;
  TestPickerClient client(&controller);

  controller.ToggleWidget();

  EXPECT_TRUE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, ToggleWidgetClosesWidgetIfOpen) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller.widget_for_testing());

  controller.ToggleWidget();

  widget_destroyed_waiter.Wait();
  EXPECT_FALSE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, ToggleWidgetShowsWidgetIfOpenedThenClosed) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();
  views::test::WidgetDestroyedWaiter widget_destroyed_waiter(
      controller.widget_for_testing());
  controller.ToggleWidget();
  widget_destroyed_waiter.Wait();

  controller.ToggleWidget();

  EXPECT_TRUE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, SetClientToNullDestroysWidgetImmediately) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();

  controller.SetClient(nullptr);

  EXPECT_FALSE(controller.widget_for_testing());
}

}  // namespace
}  // namespace ash
