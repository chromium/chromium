// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_view.h"

#include <optional>
#include <string>

#include "ash/picker/metrics/picker_session_metrics.h"
#include "ash/picker/mock_picker_asset_fetcher.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/picker/views/picker_category_view.h"
#include "ash/picker/views/picker_contents_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_search_results_view.h"
#include "ash/picker/views/picker_section_list_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/picker/views/picker_widget.h"
#include "ash/picker/views/picker_zero_state_view.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/test/test_structured_metrics_recorder.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace ash {
namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Truly;

constexpr gfx::Rect kDefaultAnchorBounds(200, 100, 0, 10);

auto ContainsEvent(const metrics::structured::Event& event) {
  return Contains(AllOf(
      Property("event name", &metrics::structured::Event::event_name,
               Eq(event.event_name())),
      Property("metric values", &metrics::structured::Event::metric_values,
               Eq(std::ref(event.metric_values())))));
}

class PickerViewTest : public AshTestBase {
 public:
  PickerViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    metrics_recorder_.Initialize();
  }

 protected:
  metrics::structured::TestStructuredMetricsRecorder metrics_recorder_;
};

class FakePickerViewDelegate : public PickerViewDelegate {
 public:
  using FakeSearchFunction =
      base::RepeatingCallback<void(SearchResultsCallback callback)>;

  struct Options {
    std::vector<PickerCategory> available_categories;
    FakeSearchFunction search_function;
  };

  explicit FakePickerViewDelegate(Options options = {}) : options_(options) {}

  std::vector<PickerCategory> GetAvailableCategories() override {
    if (options_.available_categories.empty()) {
      // Use at least one category.
      return {PickerCategory::kExpressions};
    }
    return options_.available_categories;
  }

  void TransformSelectedText(PickerCategory category) override {
    requested_case_transformation_category_ = category;
  }

  bool ShouldShowSuggestedResults() override { return true; }

  void GetResultsForCategory(PickerCategory category,
                             SearchResultsCallback callback) override {
    callback.Run({});
  }

  void StartSearch(const std::u16string& query,
                   std::optional<PickerCategory> category,
                   SearchResultsCallback callback) override {
    if (options_.search_function.is_null()) {
      std::move(callback).Run({});
    } else {
      options_.search_function.Run(std::move(callback));
    }
  }

  void InsertResultOnNextFocus(const PickerSearchResult& result) override {
    last_inserted_result_ = result;
  }
  void OpenResult(const PickerSearchResult& result) override {}

  void ShowEmojiPicker(ui::EmojiPickerCategory category,
                       std::u16string_view query) override {
    emoji_picker_query_ = std::u16string(query);
  }
  void ShowEditor(std::optional<std::string> preset_query_id,
                  std::optional<std::string> freeform_text) override {
    showed_editor_ = true;
  }
  void SetCapsLockEnabled(bool enabled) override {
    caps_lock_enabled_ = enabled;
  }
  void GetSuggestedEditorResults(
      SuggestedEditorResultsCallback callback) override {}

  PickerAssetFetcher* GetAssetFetcher() override { return &asset_fetcher_; }

  PickerSessionMetrics& GetSessionMetrics() override {
    return session_metrics_;
  }

  std::optional<PickerSearchResult> last_inserted_result() const {
    return last_inserted_result_;
  }

  std::optional<std::u16string> emoji_picker_query() const {
    return emoji_picker_query_;
  }
  bool showed_editor() const { return showed_editor_; }
  std::optional<PickerCategory> requested_case_transformation_category() const {
    return requested_case_transformation_category_;
  }
  bool caps_lock_enabled() const { return caps_lock_enabled_; }

 private:
  Options options_;
  MockPickerAssetFetcher asset_fetcher_;
  PickerSessionMetrics session_metrics_;
  std::optional<PickerSearchResult> last_inserted_result_;
  std::optional<std::u16string> emoji_picker_query_;
  bool showed_editor_ = false;
  std::optional<PickerCategory> requested_case_transformation_category_ =
      std::nullopt;
  bool caps_lock_enabled_ = false;
};

PickerView* GetPickerViewFromWidget(views::Widget& widget) {
  return views::AsViewClass<PickerView>(
      widget.non_client_view()->client_view()->children().front());
}

// Gets the first category item view that can be clicked to select a category.
PickerItemView* GetFirstCategoryItemView(PickerView* picker_view) {
  return picker_view->zero_state_view_for_testing()
      .section_views_for_testing()
      .begin()
      ->second->item_views_for_testing()[0];
}

TEST_F(PickerViewTest, BackgroundIsCorrect) {
  FakePickerViewDelegate delegate;
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  PickerView* view = GetPickerViewFromWidget(*widget);

  ASSERT_TRUE(view);
  ASSERT_TRUE(view->background());
  EXPECT_EQ(view->background()->get_color(),
            view->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysSystemBaseElevated));
}

TEST_F(PickerViewTest, SizeIsCorrect) {
  FakePickerViewDelegate delegate;
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PickerView* view = GetPickerViewFromWidget(*widget);

  EXPECT_EQ(view->size(), gfx::Size(320, 340));
}

TEST_F(PickerViewTest, ShowsZeroStateView) {
  FakePickerViewDelegate delegate({
      .available_categories = {PickerCategory::kLinks},
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
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
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
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
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
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
  {
    base::test::TestFuture<void> future;
    FakePickerViewDelegate delegate({
        .search_function = base::BindLambdaForTesting(
            [&](FakePickerViewDelegate::SearchResultsCallback callback) {
              future.SetValue();
              callback.Run({
                  PickerSearchResultsSection(
                      PickerSectionType::kExpressions,
                      {{PickerSearchResult::Text(u"result")}},
                      /*has_more_results=*/false),
              });
            }),
    });
    auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
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

  cros_events::Picker_FinishSession expected_event;
  expected_event.SetOutcome(cros_events::PickerSessionOutcome::UNKNOWN)
      .SetAction(cros_events::PickerAction::UNKNOWN)
      .SetResultSource(cros_events::PickerResultSource::UNKNOWN)
      .SetResultType(cros_events::PickerResultType::TEXT)
      .SetTotalEdits(1)
      .SetFinalQuerySize(1)
      .SetResultIndex(0);
  EXPECT_THAT(metrics_recorder_.GetEvents(), ContainsEvent(expected_event));
}

TEST_F(PickerViewTest, SwitchesToCategoryView) {
  {
    FakePickerViewDelegate delegate({
        .available_categories = {PickerCategory::kLinks},
    });
    auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
    widget->Show();

    PickerView* picker_view = GetPickerViewFromWidget(*widget);
    views::View* category_item_view = GetFirstCategoryItemView(picker_view);

    category_item_view->ScrollViewToVisible();
    ViewDrawnWaiter().Wait(category_item_view);
    LeftClickOn(category_item_view);

    EXPECT_TRUE(picker_view->category_view_for_testing().GetVisible());
    EXPECT_FALSE(picker_view->zero_state_view_for_testing().GetVisible());
    EXPECT_FALSE(picker_view->search_results_view_for_testing().GetVisible());
  }

  cros_events::Picker_FinishSession expected_event;
  expected_event.SetOutcome(cros_events::PickerSessionOutcome::UNKNOWN)
      .SetAction(cros_events::PickerAction::OPEN_LINKS)
      .SetResultSource(cros_events::PickerResultSource::UNKNOWN)
      .SetResultType(cros_events::PickerResultType::UNKNOWN)
      .SetTotalEdits(0)
      .SetFinalQuerySize(0)
      .SetResultIndex(-1);
  EXPECT_THAT(metrics_recorder_.GetEvents(), ContainsEvent(expected_event));
}

TEST_F(PickerViewTest, ClickingCategoryResultsSwitchesToCategoryView) {
  base::test::TestFuture<void> search_called;
  FakePickerViewDelegate delegate({
      .available_categories = {PickerCategory::kLinks},
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            search_called.SetValue();
            callback.Run({
                PickerSearchResultsSection(
                    PickerSectionType::kExpressions,
                    {{PickerSearchResult::Category(PickerCategory::kLinks)}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_called.Wait());

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  views::View* category_result_item_view =
      picker_view->search_results_view_for_testing()
          .section_views_for_testing()[0]
          ->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(category_result_item_view);
  LeftClickOn(category_result_item_view);

  EXPECT_TRUE(picker_view->category_view_for_testing().GetVisible());
  EXPECT_FALSE(picker_view->zero_state_view_for_testing().GetVisible());
  EXPECT_FALSE(picker_view->search_results_view_for_testing().GetVisible());
}

TEST_F(PickerViewTest, SelectingCategoryUpdatesSearchFieldPlaceholderText) {
  FakePickerViewDelegate delegate({
      .available_categories = {PickerCategory::kLinks},
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(picker_view);

  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  EXPECT_EQ(picker_view->search_field_view_for_testing()
                .textfield_for_testing()
                .GetPlaceholderText(),
            l10n_util::GetStringUTF16(
                IDS_PICKER_LINKS_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT));
}

TEST_F(PickerViewTest, SearchingWithCategorySwitchesToSearchResultsView) {
  FakePickerViewDelegate delegate({
      .available_categories = {PickerCategory::kLinks},
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  // Switch to category view.
  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(picker_view);

  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);
  // Type something into the search field.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  EXPECT_FALSE(picker_view->category_view_for_testing().GetVisible());
  EXPECT_FALSE(picker_view->zero_state_view_for_testing().GetVisible());
  EXPECT_TRUE(picker_view->search_results_view_for_testing().GetVisible());
}

TEST_F(PickerViewTest, EmptySearchFieldSwitchesBackToCategoryView) {
  FakePickerViewDelegate delegate({
      .available_categories = {PickerCategory::kLinks},
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  // Switch to category view.
  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(picker_view);

  category_item_view->ScrollViewToVisible();
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
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            search_called.SetValue();
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_called.Wait());

  // Results page should be empty until results arrive.
  EXPECT_TRUE(picker_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(picker_view->search_results_view_for_testing()
                  .section_list_view_for_testing()
                  ->children(),
              IsEmpty());
}

TEST_F(PickerViewTest, SearchingShowResultsWhenResultsArriveAsynchronously) {
  base::test::TestFuture<void> search_called;
  FakePickerViewDelegate::SearchResultsCallback search_callback;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            search_callback = std::move(callback);
            search_called.SetValue();
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_called.Wait());

  search_callback.Run({
      PickerSearchResultsSection(PickerSectionType::kExpressions, {},
                                 /*has_more_results=*/false),
  });

  EXPECT_TRUE(picker_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(
      picker_view->search_results_view_for_testing()
          .section_views_for_testing(),
      ElementsAre(Pointee(Property(
          "title", &PickerSectionView::title_label_for_testing,
          Property("text", &views::Label::GetText, u"Matching expressions")))));
}

TEST_F(PickerViewTest, SearchingKeepsOldResultsUntilNewResultsArrive) {
  base::test::TestFuture<void> search1_called;
  base::test::TestFuture<void> search2_called;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            if (!search1_called.IsReady()) {
              callback.Run({
                  PickerSearchResultsSection(PickerSectionType::kExpressions,
                                             {}, /*has_more_results=*/false),
              });
              search1_called.SetValue();
            } else {
              search2_called.SetValue();
            }
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
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
  EXPECT_THAT(
      picker_view->search_results_view_for_testing()
          .section_views_for_testing(),
      ElementsAre(Pointee(Property(
          "title", &PickerSectionView::title_label_for_testing,
          Property("text", &views::Label::GetText, u"Matching expressions")))));
}

TEST_F(PickerViewTest, SearchingReplacesOldResultsWithNewResults) {
  base::test::TestFuture<void> search1_called;
  base::test::TestFuture<void> search2_called;
  FakePickerViewDelegate::SearchResultsCallback search2_callback;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            if (!search1_called.IsReady()) {
              callback.Run({
                  PickerSearchResultsSection(PickerSectionType::kExpressions,
                                             {}, /*has_more_results=*/false),
              });
              search1_called.SetValue();
            } else {
              search2_callback = std::move(callback);
              search2_called.SetValue();
            }
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  // Go to the results page.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search1_called.Wait());
  // Start another search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search2_called.Wait());
  search2_callback.Run({
      PickerSearchResultsSection(PickerSectionType::kLinks, {},
                                 /*has_more_results=*/false),
  });

  // Results page should show the new results.
  EXPECT_TRUE(picker_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(
      picker_view->search_results_view_for_testing()
          .section_views_for_testing(),
      ElementsAre(Pointee(Property(
          "title", &PickerSectionView::title_label_for_testing,
          Property("text", &views::Label::GetText, u"Matching links")))));
}

TEST_F(PickerViewTest, ClearsResultsWhenGoingBackToZeroState) {
  base::test::TestFuture<void> search_called;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            search_called.SetValue();
            callback.Run({
                PickerSearchResultsSection(
                    PickerSectionType::kExpressions,
                    {{PickerSearchResult::Text(u"result")}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  // Go to the results page.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_called.Wait());
  // Go back to the zero state page.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);

  EXPECT_FALSE(picker_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(picker_view->search_results_view_for_testing()
                  .section_list_view_for_testing()
                  ->children(),
              IsEmpty());
}

TEST_F(PickerViewTest, PressingEscClosesPickerWidget) {
  FakePickerViewDelegate delegate;
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);

  EXPECT_TRUE(widget->IsClosed());
}

TEST_F(PickerViewTest, RecordsSearchLatencyAfterSearchFinished) {
  base::HistogramTester histogram;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&, this](FakePickerViewDelegate::SearchResultsCallback callback) {
            task_environment()->FastForwardBy(base::Seconds(1));
            callback.Run({});
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.SearchLatency",
                                   base::Seconds(1), 1);
}

TEST_F(PickerViewTest, BoundsDefaultAlignedWithAnchor) {
  FakePickerViewDelegate delegate;
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(display::Screen::GetScreen()
                  ->GetDisplayMatching(kDefaultAnchorBounds)
                  .work_area()
                  .Contains(view->GetBoundsInScreen()));
  // Should be to the right of the anchor.
  EXPECT_EQ(view->GetBoundsInScreen().x(), kDefaultAnchorBounds.right());
  // Center of the search field should be vertically aligned with the anchor.
  EXPECT_EQ(view->search_field_view_for_testing()
                .GetBoundsInScreen()
                .CenterPoint()
                .y(),
            kDefaultAnchorBounds.CenterPoint().y());
}

TEST_F(PickerViewTest, BoundsAlignedWithAnchorNearTopLeftOfScreen) {
  FakePickerViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect anchor_bounds(screen_work_area.origin(), {0, 10});
  anchor_bounds.Offset(80, 80);

  auto widget = PickerWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be to the right of the anchor.
  EXPECT_EQ(view->GetBoundsInScreen().x(), anchor_bounds.right());
  // Center of the search field should be vertically aligned with the anchor.
  EXPECT_EQ(view->search_field_view_for_testing()
                .GetBoundsInScreen()
                .CenterPoint()
                .y(),
            anchor_bounds.CenterPoint().y());
}

TEST_F(PickerViewTest, BoundsAlignedWithAnchorNearBottomLeftOfScreen) {
  FakePickerViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect anchor_bounds(screen_work_area.bottom_left(), {0, 10});
  anchor_bounds.Offset(80, -80);

  auto widget = PickerWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be to the right of the anchor.
  EXPECT_EQ(view->GetBoundsInScreen().x(), anchor_bounds.right());
  // Center of the search field should be vertically aligned with the anchor.
  EXPECT_EQ(view->search_field_view_for_testing()
                .GetBoundsInScreen()
                .CenterPoint()
                .y(),
            anchor_bounds.CenterPoint().y());
}

TEST_F(PickerViewTest, BoundsBelowAnchorForAnchorNearTopRightOfScreen) {
  FakePickerViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect anchor_bounds(screen_work_area.top_right(), {0, 10});
  anchor_bounds.Offset(-20, 20);

  auto widget = PickerWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  const PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be below the anchor.
  EXPECT_EQ(view->GetBoundsInScreen().y(), anchor_bounds.bottom());
}

TEST_F(PickerViewTest, BoundsAboveAnchorForAnchorNearBottomRightOfScreen) {
  FakePickerViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect anchor_bounds(screen_work_area.bottom_right(), {0, 10});
  anchor_bounds.Offset(-20, -20);

  auto widget = PickerWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  const PickerView* view = GetPickerViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be above the anchor.
  EXPECT_EQ(view->GetBoundsInScreen().bottom(), anchor_bounds.y());
}

TEST_F(PickerViewTest, BoundsOnScreenForEmptyAnchorBounds) {
  FakePickerViewDelegate delegate;
  auto widget = PickerWidget::Create(&delegate, gfx::Rect());
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
  gfx::Rect anchor_bounds(screen_work_area.top_center(), {0, 10});
  anchor_bounds.Offset(0, 80);

  auto widget = PickerWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  EXPECT_GE(view->contents_view_for_testing().GetBoundsInScreen().y(),
            view->search_field_view_for_testing().GetBoundsInScreen().bottom());
}

TEST_F(PickerViewTest, ResultsAboveSearchFieldNearBottomOfScreen) {
  FakePickerViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect anchor_bounds(screen_work_area.bottom_center(), {0, 10});
  anchor_bounds.Offset(0, -80);

  auto widget = PickerWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  PickerView* view = GetPickerViewFromWidget(*widget);
  EXPECT_LE(view->contents_view_for_testing().GetBoundsInScreen().bottom(),
            view->search_field_view_for_testing().GetBoundsInScreen().y());
}

TEST_F(PickerViewTest, ShowsEmojiPickerWhenClickingOnExpressions) {
  FakePickerViewDelegate delegate({
      .available_categories = {PickerCategory::kExpressions},
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  LeftClickOn(GetFirstCategoryItemView(GetPickerViewFromWidget(*widget)));

  EXPECT_TRUE(widget->IsClosed());
  EXPECT_THAT(delegate.emoji_picker_query(), Optional(Eq(u"")));
}

TEST_F(PickerViewTest, ShowsEditorWhenClickingOnEditor) {
  FakePickerViewDelegate delegate({
      .available_categories = {PickerCategory::kEditorWrite},
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  LeftClickOn(GetFirstCategoryItemView(GetPickerViewFromWidget(*widget)));

  EXPECT_TRUE(widget->IsClosed());
  EXPECT_TRUE(delegate.showed_editor());
}

TEST_F(PickerViewTest,
       CallsCasesTransformationWhenClickingOnCaseTransformation) {
  FakePickerViewDelegate delegate({
      .available_categories = {PickerCategory::kUpperCase},
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  LeftClickOn(GetFirstCategoryItemView(GetPickerViewFromWidget(*widget)));

  EXPECT_TRUE(widget->IsClosed());
  EXPECT_TRUE(delegate.requested_case_transformation_category().has_value());
  EXPECT_EQ(*delegate.requested_case_transformation_category(),
            PickerCategory::kUpperCase);
}

TEST_F(PickerViewTest, TurnsOnCapsLockWhenClickingCapsOn) {
  FakePickerViewDelegate delegate({
      .available_categories = {PickerCategory::kCapsOn},
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  LeftClickOn(GetFirstCategoryItemView(GetPickerViewFromWidget(*widget)));

  EXPECT_TRUE(delegate.caps_lock_enabled());
}

TEST_F(PickerViewTest, TurnsOffCapsLockWhenClickingCapsOff) {
  FakePickerViewDelegate delegate({
      .available_categories = {PickerCategory::kCapsOff},
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  LeftClickOn(GetFirstCategoryItemView(GetPickerViewFromWidget(*widget)));

  EXPECT_FALSE(delegate.caps_lock_enabled());
}

TEST_F(PickerViewTest, PressingEnterDoesNothingOnEmptySearchResultsPage) {
  base::test::TestFuture<void> future;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                PickerSearchResultsSection(PickerSectionType::kExpressions, {},
                                           /*has_more_results=*/false),
            });
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PickerView* view = GetPickerViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_TRUE(view->search_results_view_for_testing().GetVisible());
  EXPECT_EQ(delegate.last_inserted_result(), std::nullopt);
}

TEST_F(PickerViewTest, PressingEnterDefaultSelectsFirstSearchResult) {
  base::test::TestFuture<void> future;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                PickerSearchResultsSection(PickerSectionType::kExpressions,
                                           {{PickerSearchResult::Emoji(u"😊"),
                                             PickerSearchResult::Symbol(u"♬")}},
                                           /*has_more_results=*/false),
            });
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(PickerSearchResult::Emoji(u"😊")));
}

TEST_F(PickerViewTest, RightArrowKeyNavigatesSearchResults) {
  base::test::TestFuture<void> future;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                PickerSearchResultsSection(PickerSectionType::kExpressions,
                                           {{PickerSearchResult::Emoji(u"😊"),
                                             PickerSearchResult::Symbol(u"♬")}},
                                           /*has_more_results=*/false),
            });
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RIGHT, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(PickerSearchResult::Symbol(u"♬")));
}

TEST_F(PickerViewTest, DownArrowKeyNavigatesSearchResults) {
  base::test::TestFuture<void> future;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                PickerSearchResultsSection(
                    PickerSectionType::kCategories,
                    {{PickerSearchResult::BrowsingHistory(
                          GURL("http://foo.com"), u"Foo", ui::ImageModel()),
                      PickerSearchResult::BrowsingHistory(
                          GURL("http://bar.com"), u"Bar", ui::ImageModel())}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(PickerSearchResult::BrowsingHistory(
                  GURL("http://bar.com"), u"Bar", ui::ImageModel())));
}

TEST_F(PickerViewTest, TabKeyNavigatesSearchResults) {
  base::test::TestFuture<void> future;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                PickerSearchResultsSection(
                    PickerSectionType::kExpressions,
                    {{PickerSearchResult::Emoji(u"😊"),
                      PickerSearchResult::Symbol(u"♬"),
                      PickerSearchResult::Emoticon(u"¯\\_(ツ)_/¯")}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  ViewDrawnWaiter().Wait(GetPickerViewFromWidget(*widget)
                             ->search_results_view_for_testing()
                             .section_list_view_for_testing()
                             ->GetTopItem());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(PickerSearchResult::Symbol(u"♬")));
}

TEST_F(PickerViewTest, ShiftTabKeyNavigatesSearchResults) {
  base::test::TestFuture<void> future;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                PickerSearchResultsSection(
                    PickerSectionType::kExpressions,
                    {{PickerSearchResult::Emoji(u"😊"),
                      PickerSearchResult::Symbol(u"♬"),
                      PickerSearchResult::Emoticon(u"¯\\_(ツ)_/¯")}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  ViewDrawnWaiter().Wait(GetPickerViewFromWidget(*widget)
                             ->search_results_view_for_testing()
                             .section_list_view_for_testing()
                             ->GetTopItem());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(PickerSearchResult::Emoji(u"😊")));
}

TEST_F(PickerViewTest, ClearsSearchWhenClickingOnCategoryResult) {
  base::test::TestFuture<void> future;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                PickerSearchResultsSection(
                    PickerSectionType::kCategories,
                    {{PickerSearchResult::Category(PickerCategory::kLinks)}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  PickerView* view = GetPickerViewFromWidget(*widget);
  PickerItemView* category_result = view->search_results_view_for_testing()
                                        .section_list_view_for_testing()
                                        ->GetTopItem();
  ASSERT_TRUE(category_result);
  ViewDrawnWaiter().Wait(category_result);

  LeftClickOn(category_result);

  EXPECT_EQ(
      view->search_field_view_for_testing().textfield_for_testing().GetText(),
      u"");
}

TEST_F(PickerViewTest, PerformsCategorySearchWhenClickingOnSeeMoreResults) {
  base::test::TestFuture<void> future;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                PickerSearchResultsSection(PickerSectionType::kLinks, {},
                                           /*has_more_results=*/true),
            });
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  future.Clear();
  PickerView* view = GetPickerViewFromWidget(*widget);
  views::View* trailing_link = view->search_results_view_for_testing()
                                   .section_views_for_testing()[0]
                                   ->title_trailing_link_for_testing();

  ViewDrawnWaiter().Wait(trailing_link);
  LeftClickOn(trailing_link);

  // Should call search a second time.
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(view->search_results_view_for_testing().GetVisible());
}

TEST_F(PickerViewTest,
       OpensEmojiPickerWithQueryCategorySearchWhenClickingOnSeeMoreResults) {
  base::test::TestFuture<void> future;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                PickerSearchResultsSection(PickerSectionType::kExpressions, {},
                                           /*has_more_results=*/true),
            });
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  future.Clear();
  PickerView* view = GetPickerViewFromWidget(*widget);
  views::View* trailing_link = view->search_results_view_for_testing()
                                   .section_views_for_testing()[0]
                                   ->title_trailing_link_for_testing();

  ViewDrawnWaiter().Wait(trailing_link);
  LeftClickOn(trailing_link);

  EXPECT_TRUE(widget->IsClosed());
  EXPECT_THAT(delegate.emoji_picker_query(), Optional(Eq(u"a")));
}

TEST_F(PickerViewTest,
       KeepsSearchFieldQueryTextAndFocusWhenClickingOnSeeMoreResults) {
  base::test::TestFuture<void> future;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                PickerSearchResultsSection(PickerSectionType::kLinks, {},
                                           /*has_more_results=*/true),
            });
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  future.Clear();
  PickerView* view = GetPickerViewFromWidget(*widget);
  views::View* trailing_link = view->search_results_view_for_testing()
                                   .section_views_for_testing()[0]
                                   ->title_trailing_link_for_testing();

  ViewDrawnWaiter().Wait(trailing_link);
  LeftClickOn(trailing_link);

  EXPECT_EQ(
      view->search_field_view_for_testing().textfield_for_testing().GetText(),
      u"a");
  EXPECT_TRUE(
      view->search_field_view_for_testing().textfield_for_testing().HasFocus());
}

TEST_F(PickerViewTest, AllCategorySearchDoesNotShowNoResultsPage) {
  base::test::TestFuture<void> future;
  FakePickerViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({});
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(GetPickerViewFromWidget(*widget)
                   ->search_results_view_for_testing()
                   .no_results_view_for_testing()
                   ->GetVisible());
}

TEST_F(PickerViewTest, CategoryOnlySearchShowsNoResultsPage) {
  base::test::TestFuture<void> future;
  FakePickerViewDelegate delegate({
      .available_categories = {PickerCategory::kLinks},
      .search_function = base::BindLambdaForTesting(
          [&](FakePickerViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({});
          }),
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PickerView* picker_view = GetPickerViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(picker_view);
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(picker_view->search_results_view_for_testing()
                  .no_results_view_for_testing()
                  ->GetVisible());
}

TEST_F(PickerViewTest,
       ChangingPseudoFocusOnZeroStateNotifiesActiveDescendantChange) {
  FakePickerViewDelegate delegate({
      .available_categories = {PickerCategory::kLinks,
                               PickerCategory::kExpressions},
  });
  auto widget = PickerWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  views::test::AXEventCounter counter(views::AXEventManager::Get());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);

  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kActiveDescendantChanged), 2);
}

}  // namespace
}  // namespace ash
