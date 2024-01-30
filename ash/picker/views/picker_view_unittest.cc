// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_view.h"

#include <optional>

#include "ash/picker/mock_picker_asset_fetcher.h"
#include "ash/picker/model/picker_category.h"
#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_category_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_search_results_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_user_education_view.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/picker/views/picker_zero_state_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Truly;

constexpr gfx::Rect kDefaultCaretBounds(200, 100, 0, 10);

class PickerViewTest : public AshTestBase {
 public:
  PickerViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

class FakePickerViewDelegate : public PickerViewDelegate {
 public:
  using FakeSearchFunction = base::RepeatingCallback<PickerSearchResults(
      std::u16string_view query,
      std::optional<PickerCategory> category)>;

  FakePickerViewDelegate()
      : search_function_(
            base::BindRepeating([](std::u16string_view query,
                                   std::optional<PickerCategory> category) {
              return PickerSearchResults();
            })) {}
  explicit FakePickerViewDelegate(FakeSearchFunction search_function)
      : search_function_(search_function) {}

  std::unique_ptr<AshWebView> CreateWebView(
      const AshWebView::InitParams& params) override {
    return ash_web_view_factory_.Create(params);
  }

  void GetResultsForCategory(PickerCategory category,
                             SearchResultsCallback callback) override {
    callback.Run(PickerSearchResults());
  }

  void StartSearch(const std::u16string& query,
                   std::optional<PickerCategory> category,
                   SearchResultsCallback callback) override {
    callback.Run(search_function_.Run(query, category));
  }

  void InsertResultOnNextFocus(const PickerSearchResult& result) override {
    last_inserted_result_ = result;
  }

  bool ShouldPaint() override { return true; }

  PickerAssetFetcher* GetAssetFetcher() override { return &asset_fetcher_; }

  std::optional<PickerSearchResult> last_inserted_result() const {
    return last_inserted_result_;
  }

 private:
  TestAshWebViewFactory ash_web_view_factory_;
  FakeSearchFunction search_function_;
  MockPickerAssetFetcher asset_fetcher_;
  std::optional<PickerSearchResult> last_inserted_result_;
};

PickerView* GetPickerViewFromWidget(views::Widget& widget) {
  return views::AsViewClass<PickerView>(
      widget.non_client_view()->client_view()->children().front());
}

// Gets an item view that can be clicked to select a category.
// TODO: b/316935911 - This assumes that the picker is in the zero state and
// that the first item is a category. This probably won't be the case once more
// of the zero state view has been implemented. We should have a better way of
// getting a category item.
PickerItemView* GetCategoryItemView(PickerView* picker_view) {
  return picker_view->zero_state_view_for_testing()
      .section_views_for_testing()[0]
      ->item_views_for_testing()[0];
}

TEST_F(PickerViewTest, CreateWidgetHasCorrectHierarchy) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);

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
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);

  EXPECT_TRUE(widget->non_client_view()->frame_view()->GetBorder());
}

TEST_F(PickerViewTest, BackgroundIsCorrect) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);
  PickerView* view = GetPickerViewFromWidget(*widget);

  ASSERT_TRUE(view);
  ASSERT_TRUE(view->background());
  EXPECT_EQ(view->background()->get_color(),
            view->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysSystemBaseElevated));
}

TEST_F(PickerViewTest, SizeIsCorrect) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);
  widget->Show();
  PickerView* view = GetPickerViewFromWidget(*widget);

  EXPECT_EQ(view->size(), gfx::Size(320, 340));
}

TEST_F(PickerViewTest, ShowsZeroStateView) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);
  PickerView* view = GetPickerViewFromWidget(*widget);

  EXPECT_THAT(view->search_field_view_for_testing(),
              Property(&views::View::GetVisible, true));
  EXPECT_THAT(view->zero_state_view_for_testing(),
              Property(&views::View::GetVisible, true));
  EXPECT_THAT(view->search_results_view_for_testing(),
              Property(&views::View::GetVisible, false));
}

TEST_F(PickerViewTest, NonEmptySearchFieldContentsSwitchesToSearchResultsView) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);
  widget->Show();
  PickerView* view = GetPickerViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  EXPECT_THAT(view->zero_state_view_for_testing(),
              Property(&views::View::GetVisible, false));
  EXPECT_THAT(view->search_results_view_for_testing(),
              Property(&views::View::GetVisible, true));
}

TEST_F(PickerViewTest, EmptySearchFieldContentsSwitchesToZeroStateView) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);
  widget->Show();
  PickerView* view = GetPickerViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);

  EXPECT_THAT(view->zero_state_view_for_testing(),
              Property(&views::View::GetVisible, true));
  EXPECT_THAT(view->search_results_view_for_testing(),
              Property(&views::View::GetVisible, false));
}

TEST_F(PickerViewTest, LeftClickSearchResultSelectsResult) {
  base::test::TestFuture<void> future;
  FakePickerViewDelegate delegate(base::BindLambdaForTesting(
      [&](std::u16string_view query, std::optional<PickerCategory> category) {
        future.SetValue();
        return PickerSearchResults({{
            PickerSearchResults::Section(
                u"section", {{PickerSearchResult::Text(u"result")}}),
        }});
      }));
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);
  widget->Show();
  PickerView* view = GetPickerViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  ASSERT_THAT(
      view->search_results_view_for_testing().section_views_for_testing(),
      Not(IsEmpty()));
  ASSERT_THAT(view->search_results_view_for_testing()
                  .section_views_for_testing()[0]
                  ->item_views_for_testing(),
              Not(IsEmpty()));

  PickerItemView* result_view = view->search_results_view_for_testing()
                                    .section_views_for_testing()[0]
                                    ->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(result_view);
  LeftClickOn(result_view);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(PickerSearchResult::Text(u"result")));
}

TEST_F(PickerViewTest, SwitchesToCategoryView) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);
  widget->Show();

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  PickerItemView* category_item_view = GetCategoryItemView(picker_view);
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  EXPECT_TRUE(picker_view->category_view_for_testing().GetVisible());
  EXPECT_FALSE(picker_view->zero_state_view_for_testing().GetVisible());
  EXPECT_FALSE(picker_view->search_results_view_for_testing().GetVisible());
}

TEST_F(PickerViewTest, SearchingWithCategorySwitchesToSearchResultsView) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);
  widget->Show();

  // Switch to category view.
  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  PickerItemView* category_item_view = GetCategoryItemView(picker_view);
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);
  // Type something into the search field.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  EXPECT_FALSE(picker_view->category_view_for_testing().GetVisible());
  EXPECT_FALSE(picker_view->zero_state_view_for_testing().GetVisible());
  EXPECT_TRUE(picker_view->search_results_view_for_testing().GetVisible());
}

TEST_F(PickerViewTest, EmptySearchFieldSwitchesBackToCategoryView) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);
  widget->Show();

  // Switch to category view.
  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  PickerItemView* category_item_view = GetCategoryItemView(picker_view);
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);
  // Type something into the search field.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  // Clear the search field.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);

  EXPECT_TRUE(picker_view->category_view_for_testing().GetVisible());
  EXPECT_FALSE(picker_view->zero_state_view_for_testing().GetVisible());
  EXPECT_FALSE(picker_view->search_results_view_for_testing().GetVisible());
}

TEST_F(PickerViewTest, PressingEscClosesPickerWidget) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);

  EXPECT_TRUE(widget->IsClosed());
}

TEST_F(PickerViewTest, ClickingOutsideClosesPickerWidget) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);
  widget->Show();

  gfx::Point point_outside_widget = widget->GetWindowBoundsInScreen().origin();
  point_outside_widget.Offset(-10, -10);
  GetEventGenerator()->MoveMouseTo(point_outside_widget);
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(widget->IsClosed());
}

TEST_F(PickerViewTest, RecordsSearchLatencyAfterSearchFinished) {
  base::HistogramTester histogram;
  FakePickerViewDelegate delegate(base::BindLambdaForTesting(
      [&, this](std::u16string_view query,
                std::optional<PickerCategory> category) {
        task_environment()->FastForwardBy(base::Seconds(1));
        return PickerSearchResults();
      }));
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.SearchLatency",
                                   base::Seconds(1), 1);
}

TEST_F(PickerViewTest, BoundsDefaultAlignedWithCaret) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(kDefaultCaretBounds, &delegate);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(display::Screen::GetScreen()
                  ->GetDisplayMatching(kDefaultCaretBounds)
                  .work_area()
                  .Contains(view->GetBoundsInScreen()));
  // Should be to the right of the caret.
  EXPECT_GE(view->GetBoundsInScreen().x(), kDefaultCaretBounds.right());
  // Center of the search field should be vertically aligned with the caret.
  EXPECT_EQ(view->search_field_view_for_testing()
                .GetBoundsInScreen()
                .CenterPoint()
                .y(),
            kDefaultCaretBounds.CenterPoint().y());
}

TEST_F(PickerViewTest, BoundsAlignedWithCaretNearTopLeftOfScreen) {
  FakePickerViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect caret_bounds(screen_work_area.origin(), {0, 10});
  caret_bounds.Offset(80, 80);

  auto widget = PickerView::CreateWidget(caret_bounds, &delegate);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be to the right of the caret.
  EXPECT_GE(view->GetBoundsInScreen().x(), caret_bounds.right());
  // Center of the search field should be vertically aligned with the caret.
  EXPECT_EQ(view->search_field_view_for_testing()
                .GetBoundsInScreen()
                .CenterPoint()
                .y(),
            caret_bounds.CenterPoint().y());
}

TEST_F(PickerViewTest, BoundsAlignedWithCaretNearBottomLeftOfScreen) {
  FakePickerViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect caret_bounds(screen_work_area.bottom_left(), {0, 10});
  caret_bounds.Offset(80, -80);

  auto widget = PickerView::CreateWidget(caret_bounds, &delegate);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be to the right of the caret.
  EXPECT_GE(view->GetBoundsInScreen().x(), caret_bounds.right());
  // Center of the search field should be vertically aligned with the caret.
  EXPECT_EQ(view->search_field_view_for_testing()
                .GetBoundsInScreen()
                .CenterPoint()
                .y(),
            caret_bounds.CenterPoint().y());
}

TEST_F(PickerViewTest, BoundsBelowCaretForCaretNearTopRightOfScreen) {
  FakePickerViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect caret_bounds(screen_work_area.top_right(), {0, 10});
  caret_bounds.Offset(-20, 20);

  auto widget = PickerView::CreateWidget(caret_bounds, &delegate);
  widget->Show();

  const PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be below the caret.
  EXPECT_GE(view->GetBoundsInScreen().y(), caret_bounds.bottom());
}

TEST_F(PickerViewTest, BoundsAboveCaretForCaretNearBottomRightOfScreen) {
  FakePickerViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect caret_bounds(screen_work_area.bottom_right(), {0, 10});
  caret_bounds.Offset(-20, -20);

  auto widget = PickerView::CreateWidget(caret_bounds, &delegate);
  widget->Show();

  const PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be above the caret.
  EXPECT_LE(view->GetBoundsInScreen().bottom(), caret_bounds.y());
}

TEST_F(PickerViewTest, BoundsOnScreenForEmptyCaretBounds) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(gfx::Rect(), &delegate);
  widget->Show();

  const PickerView* view = GetPickerViewFromWidget(*widget);
  EXPECT_TRUE(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().Contains(
          view->GetBoundsInScreen()));
}

TEST_F(PickerViewTest, ResultsBelowSearchFieldNearTopOfScreen) {
  FakePickerViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect caret_bounds(screen_work_area.top_center(), {0, 10});
  caret_bounds.Offset(0, 80);

  auto widget = PickerView::CreateWidget(caret_bounds, &delegate);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  EXPECT_GE(view->zero_state_view_for_testing().GetBoundsInScreen().y(),
            view->search_field_view_for_testing().GetBoundsInScreen().bottom());
}

TEST_F(PickerViewTest, ResultsAboveSearchFieldNearBottomOfScreen) {
  FakePickerViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect caret_bounds(screen_work_area.bottom_center(), {0, 10});
  caret_bounds.Offset(0, -80);

  auto widget = PickerView::CreateWidget(caret_bounds, &delegate);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  EXPECT_LE(view->zero_state_view_for_testing().GetBoundsInScreen().bottom(),
            view->search_field_view_for_testing().GetBoundsInScreen().y());
}

}  // namespace
}  // namespace ash
