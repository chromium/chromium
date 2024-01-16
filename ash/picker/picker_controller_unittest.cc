// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_controller.h"

#include "ash/picker/model/picker_search_results.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace {

class PickerControllerTest : public AshTestBase {
 public:
  PickerControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

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

  void DownloadGifToString(const GURL& url,
                           DownloadGifToStringCallback callback) override {}

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

TEST_F(PickerControllerTest, SetClientToNullKeepsWidget) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();

  controller.SetClient(nullptr);

  EXPECT_TRUE(controller.widget_for_testing());
}

TEST_F(PickerControllerTest, ShowWidgetRecordsInputReadyLatency) {
  base::HistogramTester histogram;
  PickerController controller;
  TestPickerClient client(&controller);

  controller.ToggleWidget(base::TimeTicks::Now());
  views::test::WidgetVisibleWaiter widget_visible_waiter(
      controller.widget_for_testing());
  widget_visible_waiter.Wait();

  histogram.ExpectTotalCount("Ash.Picker.Session.InputReadyLatency", 1);
}

TEST_F(PickerControllerTest, InsertResultDoesNothingWhenWidgetIsClosed) {
  PickerController controller;
  TestPickerClient client(&controller);
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller.InsertResultOnNextFocus(PickerSearchResult::Text(u"abc"));
  ui::FakeTextInputClient input_field(ui::TEXT_INPUT_TYPE_TEXT);
  input_method->SetFocusedTextInputClient(&input_field);
  absl::Cleanup focused_input_field_reset = [input_method] {
    // Reset the input field since it will be destroyed before `input_method`.
    input_method->SetFocusedTextInputClient(nullptr);
  };

  EXPECT_EQ(input_field.text(), u"");
}

TEST_F(PickerControllerTest, InsertResultInsertsIntoInputFieldAfterFocus) {
  PickerController controller;
  TestPickerClient client(&controller);
  controller.ToggleWidget();
  auto* input_method =
      Shell::GetPrimaryRootWindow()->GetHost()->GetInputMethod();

  controller.InsertResultOnNextFocus(PickerSearchResult::Text(u"abc"));
  controller.widget_for_testing()->CloseNow();
  ui::FakeTextInputClient input_field(ui::TEXT_INPUT_TYPE_TEXT);
  input_method->SetFocusedTextInputClient(&input_field);
  absl::Cleanup focused_input_field_reset = [input_method] {
    // Reset the input field since it will be destroyed before `input_method`.
    input_method->SetFocusedTextInputClient(nullptr);
  };

  EXPECT_EQ(input_field.text(), u"abc");
}

}  // namespace
}  // namespace ash
