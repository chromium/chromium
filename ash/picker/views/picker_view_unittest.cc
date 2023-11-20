// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_view.h"

#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view.h"
#include "ash/test/test_ash_web_view_factory.h"
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

class FakePickerViewDelegate : public PickerView::Delegate {
 public:
  std::unique_ptr<AshWebView> CreateWebView(
      const AshWebView::InitParams& params) override {
    return ash_web_view_factory_.Create(params);
  }

 private:
  TestAshWebViewFactory ash_web_view_factory_;
};

PickerView* GetPickerViewFromWidget(views::Widget& widget) {
  return views::AsViewClass<PickerView>(
      widget.non_client_view()->client_view()->children().front());
}

TEST_F(PickerViewTest, CreateWidgetHasCorrectHierarchy) {
  auto widget =
      PickerView::CreateWidget(std::make_unique<FakePickerViewDelegate>());

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
  auto widget =
      PickerView::CreateWidget(std::make_unique<FakePickerViewDelegate>());

  EXPECT_TRUE(widget->non_client_view()->frame_view()->GetBorder());
}

TEST_F(PickerViewTest, BackgroundIsCorrect) {
  auto widget =
      PickerView::CreateWidget(std::make_unique<FakePickerViewDelegate>());
  PickerView* view = GetPickerViewFromWidget(*widget);

  ASSERT_TRUE(view);
  ASSERT_TRUE(view->background());
  EXPECT_EQ(
      view->background()->get_color(),
      view->GetColorProvider()->GetColor(cros_tokens::kCrosSysBaseElevated));
}

TEST_F(PickerViewTest, SizeIsCorrect) {
  auto widget =
      PickerView::CreateWidget(std::make_unique<FakePickerViewDelegate>());
  widget->Show();
  PickerView* view = GetPickerViewFromWidget(*widget);

  EXPECT_EQ(view->size(), gfx::Size(420, 480));
}

TEST_F(PickerViewTest, CreatesWebViewToWebUI) {
  auto widget =
      PickerView::CreateWidget(std::make_unique<FakePickerViewDelegate>());
  PickerView* view = GetPickerViewFromWidget(*widget);

  const auto& web_view =
      static_cast<const TestAshWebView&>(view->web_view_for_testing());
  EXPECT_EQ(web_view.current_url(), GURL("chrome://picker"));
}

}  // namespace
}  // namespace ash
