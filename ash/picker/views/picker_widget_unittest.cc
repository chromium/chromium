// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_widget.h"

#include <memory>

#include "ash/picker/views/picker_view.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {
namespace {

using ::testing::ElementsAre;
using ::testing::Truly;

constexpr gfx::Rect kDefaultAnchorBounds(200, 100, 0, 10);

class FakePickerViewDelegate : public PickerViewDelegate {
 public:
  // PickerViewDelegate:
  std::unique_ptr<AshWebView> CreateWebView(
      const AshWebView::InitParams& params) override {
    return nullptr;
  }
  void GetResultsForCategory(PickerCategory category,
                             SearchResultsCallback callback) override {}
  void StartSearch(const std::u16string& query,
                   std::optional<PickerCategory> category,
                   SearchResultsCallback callback) override {}
  void InsertResultOnNextFocus(const PickerSearchResult& result) override {}
  void ShowEmojiPicker(ui::EmojiPickerCategory category) override {}
  PickerAssetFetcher* GetAssetFetcher() override { return nullptr; }
};

using PickerWidgetTest = AshTestBase;

TEST_F(PickerWidgetTest, CreateWidgetHasCorrectHierarchy) {
  FakePickerViewDelegate delegate;
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);

  // Widget should contain a NonClientView, which has a NonClientFrameView for
  // borders and shadows, and a ClientView with a sole child of the PickerView.
  ASSERT_TRUE(widget);
  ASSERT_TRUE(widget->non_client_view());
  ASSERT_TRUE(widget->non_client_view()->frame_view());
  ASSERT_TRUE(widget->non_client_view()->client_view());
  EXPECT_THAT(widget->non_client_view()->client_view()->children(),
              ElementsAre(Truly(views::IsViewClass<PickerView>)));
}

TEST_F(PickerWidgetTest, CreateWidgetHasCorrectBorder) {
  FakePickerViewDelegate delegate;
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);

  EXPECT_TRUE(widget->non_client_view()->frame_view()->GetBorder());
}

TEST_F(PickerWidgetTest, ClickingOutsideClosesPickerWidget) {
  FakePickerViewDelegate delegate;
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  gfx::Point point_outside_widget = widget->GetWindowBoundsInScreen().origin();
  point_outside_widget.Offset(-10, -10);
  GetEventGenerator()->MoveMouseTo(point_outside_widget);
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(widget->IsClosed());
}

TEST_F(PickerWidgetTest, LosingFocusClosesPickerWidget) {
  // Create something other than the picker to focus.
  auto window = CreateTestWindow();
  window->Show();

  // Create the fake picker and make sure it has focus.
  FakePickerViewDelegate delegate;
  auto picker_widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  picker_widget->Show();
  EXPECT_THAT(picker_widget->GetFocusManager()->GetFocusedView(),
              testing::NotNull());

  // Focus the other Widget and expect the picker to have closed.
  window->Focus();
  EXPECT_TRUE(window->HasFocus());

  EXPECT_TRUE(picker_widget->IsClosed());
}

}  // namespace
}  // namespace ash
