// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_widget.h"

#include <memory>
#include <string_view>

#include "ash/quick_insert/metrics/quick_insert_session_metrics.h"
#include "ash/quick_insert/model/quick_insert_action_type.h"
#include "ash/quick_insert/model/quick_insert_caps_lock_position.h"
#include "ash/quick_insert/views/quick_insert_preview_bubble.h"
#include "ash/quick_insert/views/quick_insert_view.h"
#include "ash/quick_insert/views/quick_insert_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
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

class FakeQuickInsertViewDelegate : public QuickInsertViewDelegate {
 public:
  // QuickInsertViewDelegate:
  std::vector<QuickInsertCategory> GetAvailableCategories() override {
    return {};
  }
  void GetZeroStateSuggestedResults(
      SuggestedResultsCallback callback) override {}
  void GetResultsForCategory(QuickInsertCategory category,
                             SearchResultsCallback callback) override {}
  void StartSearch(std::u16string_view query,
                   std::optional<QuickInsertCategory> category,
                   SearchResultsCallback callback) override {}
  void StopSearch() override {}
  void StartEmojiSearch(std::u16string_view query,
                        EmojiSearchResultsCallback callback) override {}
  void CloseWidgetThenInsertResultOnNextFocus(
      const QuickInsertSearchResult& result) override {}
  void OpenResult(const QuickInsertSearchResult& result) override {}
  void ShowEmojiPicker(ui::EmojiPickerCategory category,
                       std::u16string_view query) override {}
  void ShowEditor(std::optional<std::string> preset_query_id,
                  std::optional<std::string> freeform_text) override {}
  void ShowLobster(std::optional<std::string> freeform_text) override {}
  PickerAssetFetcher* GetAssetFetcher() override { return nullptr; }
  PickerSessionMetrics& GetSessionMetrics() override {
    return session_metrics_;
  }
  QuickInsertActionType GetActionForResult(
      const QuickInsertSearchResult& result) override {
    return QuickInsertActionType::kInsert;
  }
  std::vector<QuickInsertEmojiResult> GetSuggestedEmoji() override {
    return {};
  }
  bool IsGifsEnabled() override { return true; }
  PickerModeType GetMode() override { return PickerModeType::kNoSelection; }
  PickerCapsLockPosition GetCapsLockPosition() override {
    return PickerCapsLockPosition::kTop;
  }

 private:
  PickerSessionMetrics session_metrics_;
};

using QuickInsertWidgetTest = AshTestBase;

TEST_F(QuickInsertWidgetTest, CreateWidgetHasCorrectHierarchy) {
  FakeQuickInsertViewDelegate delegate;
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);

  // Widget should contain a NonClientView, which has a NonClientFrameView for
  // borders and shadows, and a ClientView with a sole child of the
  // QuickInsertView.
  ASSERT_TRUE(widget);
  ASSERT_TRUE(widget->non_client_view());
  ASSERT_TRUE(widget->non_client_view()->frame_view());
  ASSERT_TRUE(widget->non_client_view()->client_view());
  EXPECT_THAT(widget->non_client_view()->client_view()->children(),
              ElementsAre(Truly(views::IsViewClass<QuickInsertView>)));
}

TEST_F(QuickInsertWidgetTest, CreateWidgetHasCorrectBorder) {
  FakeQuickInsertViewDelegate delegate;
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);

  EXPECT_TRUE(widget->non_client_view()->frame_view()->GetBorder());
}

TEST_F(QuickInsertWidgetTest, ClickingOutsideClosesQuickInsertWidget) {
  FakeQuickInsertViewDelegate delegate;
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  gfx::Point point_outside_widget = widget->GetWindowBoundsInScreen().origin();
  point_outside_widget.Offset(-10, -10);
  GetEventGenerator()->MoveMouseTo(point_outside_widget);
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(widget->IsClosed());
}

TEST_F(QuickInsertWidgetTest, LosingFocusClosesQuickInsertWidget) {
  // Create something other than the picker to focus.
  auto window = CreateTestWindow();
  window->Show();

  // Create the fake picker and make sure it has focus.
  FakeQuickInsertViewDelegate delegate;
  auto quick_insert_widget =
      QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  quick_insert_widget->Show();
  EXPECT_THAT(quick_insert_widget->GetFocusManager()->GetFocusedView(),
              testing::NotNull());

  // Focus the other Widget and expect the picker to have closed.
  window->Focus();
  EXPECT_TRUE(window->HasFocus());

  EXPECT_TRUE(quick_insert_widget->IsClosed());
  EXPECT_EQ(delegate.GetSessionMetrics().GetOutcomeForTesting(),
            PickerSessionMetrics::SessionOutcome::kAbandoned);
}

TEST_F(QuickInsertWidgetTest, PreviewBubbleDoesNotStealFocusQuickInsertWidget) {
  std::unique_ptr<views::Widget> anchor_widget = CreateFramelessTestWidget();
  anchor_widget->SetContentsView(std::make_unique<views::View>());

  // Create the QuickInsertWidget and make sure it has focus.
  FakeQuickInsertViewDelegate delegate;
  auto quick_insert_widget =
      QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  quick_insert_widget->Show();

  // Show bubble widget and expect the QuickInsertWidget to not close.
  views::View* bubble_view =
      new PickerPreviewBubbleView(anchor_widget->GetContentsView());
  bubble_view->GetWidget()->Show();

  EXPECT_FALSE(quick_insert_widget->IsClosed());

  bubble_view->GetWidget()->CloseNow();
}

TEST_F(QuickInsertWidgetTest, CreatesCenteredWidget) {
  FakeQuickInsertViewDelegate delegate;
  auto widget =
      QuickInsertWidget::CreateCentered(&delegate, gfx::Rect(10, 10, 10, 10));
  widget->Show();

  EXPECT_EQ(widget->GetWindowBoundsInScreen().CenterPoint(),
            display::Screen::GetScreen()
                ->GetPrimaryDisplay()
                .work_area()
                .CenterPoint());
}

}  // namespace
}  // namespace ash
