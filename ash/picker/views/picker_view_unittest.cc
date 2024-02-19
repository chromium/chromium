// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_view.h"

#include <optional>

#include "ash/picker/mock_picker_asset_fetcher.h"
#include "ash/picker/model/picker_category.h"
#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/picker/views/picker_category_view.h"
#include "ash/picker/views/picker_contents_view.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_search_results_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/picker/views/picker_zero_state_view.h"
#include "ash/strings/grit/ash_strings.h"
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
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Truly;

constexpr gfx::Rect kDefaultCaretBounds(200, 100, 0, 10);
constexpr gfx::Point kDefaultCursorPoint(300, 400);
constexpr gfx::Rect kDefaultFocusedWindowBounds(300, 400);

class PickerViewTest : public AshTestBase {
 public:
  PickerViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

class FakePickerViewDelegate : public PickerViewDelegate {
 public:
  using FakeSearchFunction =
      base::RepeatingCallback<void(SearchResultsCallback callback)>;

  FakePickerViewDelegate()
      : search_function_(
            base::BindRepeating([](SearchResultsCallback callback) {
              callback.Run(PickerSearchResults());
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
    search_function_.Run(std::move(callback));
  }

  void InsertResultOnNextFocus(const PickerSearchResult& result) override {
    last_inserted_result_ = result;
  }

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
views::View* GetCategoryItemView(PickerView* picker_view) {
  return picker_view->zero_state_view_for_testing()
      .section_views_for_testing()
      .find(PickerCategoryType::kExpressions)
      ->second->item_views_for_testing()[0];
}
views::View* GetNonEmojiCategoryItemView(PickerView* picker_view) {
  return picker_view->zero_state_view_for_testing()
      .section_views_for_testing()
      .find(PickerCategoryType::kExpressions)
      ->second->item_views_for_testing()[1];
}

TEST_F(PickerViewTest, CreateWidgetHasCorrectHierarchy) {
  FakePickerViewDelegate delegate;
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);

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
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);

  EXPECT_TRUE(widget->non_client_view()->frame_view()->GetBorder());
}

TEST_F(PickerViewTest, BackgroundIsCorrect) {
  FakePickerViewDelegate delegate;
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  PickerView* view = GetPickerViewFromWidget(*widget);

  ASSERT_TRUE(view);
  ASSERT_TRUE(view->background());
  EXPECT_EQ(view->background()->get_color(),
            view->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysSystemBaseElevated));
}

TEST_F(PickerViewTest, SizeIsCorrect) {
  FakePickerViewDelegate delegate;
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();
  PickerView* view = GetPickerViewFromWidget(*widget);

  EXPECT_EQ(view->size(), gfx::Size(320, 340));
}

TEST_F(PickerViewTest, ShowsZeroStateView) {
  FakePickerViewDelegate delegate;
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
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
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
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
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
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
      [&](FakePickerViewDelegate::SearchResultsCallback callback) {
        future.SetValue();
        callback.Run(PickerSearchResults({{
            PickerSearchResults::Section(
                u"section", {{PickerSearchResult::Text(u"result")}}),
        }}));
      }));
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
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

  views::View* result_view = view->search_results_view_for_testing()
                                 .section_views_for_testing()[0]
                                 ->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(result_view);
  LeftClickOn(result_view);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(PickerSearchResult::Text(u"result")));
}

TEST_F(PickerViewTest, SwitchesToCategoryView) {
  FakePickerViewDelegate delegate;
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  views::View* category_item_view = GetNonEmojiCategoryItemView(picker_view);
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  EXPECT_TRUE(picker_view->category_view_for_testing().GetVisible());
  EXPECT_FALSE(picker_view->zero_state_view_for_testing().GetVisible());
  EXPECT_FALSE(picker_view->search_results_view_for_testing().GetVisible());
}

TEST_F(PickerViewTest, SelectingCategoryUpdatesSearchFieldPlaceholderText) {
  FakePickerViewDelegate delegate;
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  views::View* category_item_view = GetNonEmojiCategoryItemView(picker_view);
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  EXPECT_THAT(picker_view->search_field_view_for_testing()
                  .textfield_for_testing()
                  .GetPlaceholderText(),
              Eq(l10n_util::GetStringUTF16(
                  IDS_PICKER_SYMBOLS_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT)));
}

TEST_F(PickerViewTest, SearchingWithCategorySwitchesToSearchResultsView) {
  FakePickerViewDelegate delegate;
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();

  // Switch to category view.
  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  views::View* category_item_view = GetNonEmojiCategoryItemView(picker_view);
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
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();

  // Switch to category view.
  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  views::View* category_item_view = GetNonEmojiCategoryItemView(picker_view);
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

TEST_F(PickerViewTest, SearchingShowEmptyResultsWhenNoResultsArriveYet) {
  base::test::TestFuture<void> search_called;
  FakePickerViewDelegate delegate(base::BindLambdaForTesting(
      [&](FakePickerViewDelegate::SearchResultsCallback callback) {
        search_called.SetValue();
      }));
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_called.Wait());

  // Results page should be empty until results arrive.
  EXPECT_TRUE(picker_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(picker_view->search_results_view_for_testing().children(),
              IsEmpty());
}

TEST_F(PickerViewTest, SearchingShowResultsWhenResultsArriveAsynchronously) {
  base::test::TestFuture<void> search_called;
  FakePickerViewDelegate::SearchResultsCallback search_callback;
  FakePickerViewDelegate delegate(base::BindLambdaForTesting(
      [&](FakePickerViewDelegate::SearchResultsCallback callback) {
        search_callback = std::move(callback);
        search_called.SetValue();
      }));
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_called.Wait());

  search_callback.Run(PickerSearchResults({{
      PickerSearchResults::Section(u"section", {}),
  }}));

  EXPECT_TRUE(picker_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(picker_view->search_results_view_for_testing()
                  .section_views_for_testing(),
              ElementsAre(Pointee(Property(
                  "title", &PickerSectionView::title_label_for_testing,
                  Property("text", &views::Label::GetText, u"section")))));
}

TEST_F(PickerViewTest, SearchingKeepsOldResultsUntilNewResultsArrive) {
  base::test::TestFuture<void> search1_called;
  base::test::TestFuture<void> search2_called;
  FakePickerViewDelegate delegate(base::BindLambdaForTesting(
      [&](FakePickerViewDelegate::SearchResultsCallback callback) {
        if (!search1_called.IsReady()) {
          callback.Run(PickerSearchResults({{
              PickerSearchResults::Section(u"section", {}),
          }}));
          search1_called.SetValue();
        } else {
          search2_called.SetValue();
        }
      }));
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  // Go to the results page.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search1_called.Wait());
  // Start another search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search2_called.Wait());

  // Results page should keep old results until new results arrive.
  EXPECT_TRUE(picker_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(picker_view->search_results_view_for_testing()
                  .section_views_for_testing(),
              ElementsAre(Pointee(Property(
                  "title", &PickerSectionView::title_label_for_testing,
                  Property("text", &views::Label::GetText, u"section")))));
}

TEST_F(PickerViewTest, SearchingReplacesOldResultsWithNewResults) {
  base::test::TestFuture<void> search1_called;
  base::test::TestFuture<void> search2_called;
  FakePickerViewDelegate::SearchResultsCallback search2_callback;
  FakePickerViewDelegate delegate(base::BindLambdaForTesting(
      [&](FakePickerViewDelegate::SearchResultsCallback callback) {
        if (!search1_called.IsReady()) {
          callback.Run(PickerSearchResults({{
              PickerSearchResults::Section(u"section", {}),
          }}));
          search1_called.SetValue();
        } else {
          search2_callback = std::move(callback);
          search2_called.SetValue();
        }
      }));
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  // Go to the results page.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search1_called.Wait());
  // Start another search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search2_called.Wait());
  search2_callback.Run(PickerSearchResults({{
      PickerSearchResults::Section(u"section2", {}),
  }}));

  // Results page should show the new results.
  EXPECT_TRUE(picker_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(picker_view->search_results_view_for_testing()
                  .section_views_for_testing(),
              ElementsAre(Pointee(Property(
                  "title", &PickerSectionView::title_label_for_testing,
                  Property("text", &views::Label::GetText, u"section2")))));
}

TEST_F(PickerViewTest, ClearsResultsWhenGoingBackToZeroState) {
  base::test::TestFuture<void> search_called;
  FakePickerViewDelegate delegate(base::BindLambdaForTesting(
      [&](FakePickerViewDelegate::SearchResultsCallback callback) {
        search_called.SetValue();
        callback.Run(PickerSearchResults({{
            PickerSearchResults::Section(
                u"section", {{PickerSearchResult::Text(u"result")}}),
        }}));
      }));
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  // Go to the results page.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_called.Wait());
  // Go back to the zero state page.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);

  EXPECT_FALSE(picker_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(picker_view->search_results_view_for_testing().children(),
              IsEmpty());
}

TEST_F(PickerViewTest, PressingEscClosesPickerWidget) {
  FakePickerViewDelegate delegate;
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);

  EXPECT_TRUE(widget->IsClosed());
}

TEST_F(PickerViewTest, ClickingOutsideClosesPickerWidget) {
  FakePickerViewDelegate delegate;
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
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
      [&, this](FakePickerViewDelegate::SearchResultsCallback callback) {
        task_environment()->FastForwardBy(base::Seconds(1));
        callback.Run(PickerSearchResults());
      }));
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.SearchLatency",
                                   base::Seconds(1), 1);
}

TEST_F(PickerViewTest, BoundsDefaultAlignedWithCaret) {
  FakePickerViewDelegate delegate;
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(display::Screen::GetScreen()
                  ->GetDisplayMatching(kDefaultCaretBounds)
                  .work_area()
                  .Contains(view->GetBoundsInScreen()));
  // Should be to the right of the caret.
  EXPECT_GT(view->GetBoundsInScreen().x(), kDefaultCaretBounds.right());
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

  auto widget = PickerView::CreateWidget(caret_bounds, kDefaultCursorPoint,
                                         screen_work_area, &delegate);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be to the right of the caret.
  EXPECT_GT(view->GetBoundsInScreen().x(), caret_bounds.right());
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

  auto widget = PickerView::CreateWidget(caret_bounds, kDefaultCursorPoint,
                                         screen_work_area, &delegate);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be to the right of the caret.
  EXPECT_GT(view->GetBoundsInScreen().x(), caret_bounds.right());
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

  auto widget = PickerView::CreateWidget(caret_bounds, kDefaultCursorPoint,
                                         screen_work_area, &delegate);
  widget->Show();

  const PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be below the caret.
  EXPECT_GT(view->GetBoundsInScreen().y(), caret_bounds.bottom());
}

TEST_F(PickerViewTest, BoundsAboveCaretForCaretNearBottomRightOfScreen) {
  FakePickerViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect caret_bounds(screen_work_area.bottom_right(), {0, 10});
  caret_bounds.Offset(-20, -20);

  auto widget = PickerView::CreateWidget(caret_bounds, kDefaultCursorPoint,
                                         screen_work_area, &delegate);
  widget->Show();

  const PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be above the caret.
  EXPECT_LT(view->GetBoundsInScreen().bottom(), caret_bounds.y());
}

TEST_F(PickerViewTest, BoundsAlignedWithCursorForEmptyCaretBounds) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(
      gfx::Rect(), kDefaultCursorPoint, kDefaultFocusedWindowBounds, &delegate);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(display::Screen::GetScreen()
                  ->GetDisplayNearestPoint(kDefaultCursorPoint)
                  .work_area()
                  .Contains(view->GetBoundsInScreen()));
  // Should be to the right of the cursor.
  EXPECT_GE(view->GetBoundsInScreen().x(), kDefaultCursorPoint.x());
  // Center of the search field should be vertically aligned with the cursor.
  EXPECT_EQ(view->search_field_view_for_testing()
                .GetBoundsInScreen()
                .CenterPoint()
                .y(),
            kDefaultCursorPoint.y());
}

TEST_F(PickerViewTest, BoundsAlignedWithCursorForCaretOutsideFocusedWindow) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(
      /*caret_bounds=*/gfx::Rect(10, 10, 0, 10), kDefaultCursorPoint,
      /*focused_window_bounds=*/gfx::Rect(100, 100, 300, 300), &delegate);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(display::Screen::GetScreen()
                  ->GetDisplayNearestPoint(kDefaultCursorPoint)
                  .work_area()
                  .Contains(view->GetBoundsInScreen()));
  // Should be to the right of the cursor.
  EXPECT_GE(view->GetBoundsInScreen().x(), kDefaultCursorPoint.x());
  // Center of the search field should be vertically aligned with the cursor.
  EXPECT_EQ(view->search_field_view_for_testing()
                .GetBoundsInScreen()
                .CenterPoint()
                .y(),
            kDefaultCursorPoint.y());
}

TEST_F(PickerViewTest, BoundsOnScreenForEmptyCaretAndEmptyCursor) {
  FakePickerViewDelegate delegate;
  auto widget = PickerView::CreateWidget(
      gfx::Rect(), gfx::Point(), kDefaultFocusedWindowBounds, &delegate);
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

  auto widget = PickerView::CreateWidget(caret_bounds, kDefaultCursorPoint,
                                         screen_work_area, &delegate);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  EXPECT_GE(view->contents_view_for_testing().GetBoundsInScreen().y(),
            view->search_field_view_for_testing().GetBoundsInScreen().bottom());
}

TEST_F(PickerViewTest, ResultsAboveSearchFieldNearBottomOfScreen) {
  FakePickerViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect caret_bounds(screen_work_area.bottom_center(), {0, 10});
  caret_bounds.Offset(0, -80);

  auto widget = PickerView::CreateWidget(caret_bounds, kDefaultCursorPoint,
                                         screen_work_area, &delegate);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  EXPECT_LE(view->contents_view_for_testing().GetBoundsInScreen().bottom(),
            view->search_field_view_for_testing().GetBoundsInScreen().y());
}

TEST_F(PickerViewTest, ShowsEmojiPickerWhenClickingOnEmoji) {
  FakePickerViewDelegate delegate;
  auto widget =
      PickerView::CreateWidget(kDefaultCaretBounds, kDefaultCursorPoint,
                               kDefaultFocusedWindowBounds, &delegate);
  widget->Show();
  bool called = false;
  ui::SetShowEmojiKeyboardCallback(base::BindLambdaForTesting(
      [&](ui::EmojiPickerCategory) { called = true; }));

  LeftClickOn(GetCategoryItemView(GetPickerViewFromWidget(*widget)));

  EXPECT_TRUE(widget->IsClosed());
  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace ash
