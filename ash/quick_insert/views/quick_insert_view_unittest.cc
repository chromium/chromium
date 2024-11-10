// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_view.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/quick_insert/metrics/quick_insert_session_metrics.h"
#include "ash/quick_insert/mock_quick_insert_asset_fetcher.h"
#include "ash/quick_insert/model/quick_insert_action_type.h"
#include "ash/quick_insert/model/quick_insert_caps_lock_position.h"
#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_controller.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/views/quick_insert_category_type.h"
#include "ash/quick_insert/views/quick_insert_contents_view.h"
#include "ash/quick_insert/views/quick_insert_emoji_bar_view.h"
#include "ash/quick_insert/views/quick_insert_emoji_item_view.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/quick_insert/views/quick_insert_preview_bubble.h"
#include "ash/quick_insert/views/quick_insert_preview_bubble_controller.h"
#include "ash/quick_insert/views/quick_insert_search_bar_textfield.h"
#include "ash/quick_insert/views/quick_insert_search_field_view.h"
#include "ash/quick_insert/views/quick_insert_search_results_view.h"
#include "ash/quick_insert/views/quick_insert_section_list_view.h"
#include "ash/quick_insert/views/quick_insert_section_view.h"
#include "ash/quick_insert/views/quick_insert_style.h"
#include "ash/quick_insert/views/quick_insert_submenu_controller.h"
#include "ash/quick_insert/views/quick_insert_submenu_view.h"
#include "ash/quick_insert/views/quick_insert_view_delegate.h"
#include "ash/quick_insert/views/quick_insert_widget.h"
#include "ash/quick_insert/views/quick_insert_zero_state_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
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
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {
namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Truly;
using ::testing::VariantWith;

constexpr gfx::Rect kDefaultAnchorBounds(200, 100, 0, 10);

template <class V, class Matcher>
auto AsView(Matcher matcher) {
  return ResultOf(
      "AsViewClass",
      [](views::View* view) { return views::AsViewClass<V>(view); },
      Pointee(matcher));
}

auto ContainsEvent(const metrics::structured::Event& event) {
  return Contains(AllOf(
      Property("event name", &metrics::structured::Event::event_name,
               Eq(event.event_name())),
      Property("metric values", &metrics::structured::Event::metric_values,
               Eq(std::ref(event.metric_values())))));
}

class PickerPreviewBubbleVisibleWaiter
    : public PickerPreviewBubbleController::Observer {
 public:
  void Wait(PickerPreviewBubbleController* preview_controller) {
    if (!preview_controller->IsBubbleVisible()) {
      preview_bubble_observation_.Observe(preview_controller);
      run_loop_.Run();
    }
  }

  void OnPreviewBubbleVisibilityChanged(bool visible) override {
    if (visible) {
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<PickerPreviewBubbleController,
                          PickerPreviewBubbleController::Observer>
      preview_bubble_observation_{this};
};

class QuickInsertViewTest : public AshTestBase {
 public:
  QuickInsertViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    metrics_recorder_.Initialize();
  }

 protected:
  metrics::structured::TestStructuredMetricsRecorder metrics_recorder_;
};

// QuickInsertViewTest parameterized by the Emoji Category.
class QuickInsertViewEmojiTest
    : public QuickInsertViewTest,
      public testing::WithParamInterface<QuickInsertCategory> {};

INSTANTIATE_TEST_SUITE_P(,
                         QuickInsertViewEmojiTest,
                         testing::ValuesIn({QuickInsertCategory::kEmojisGifs,
                                            QuickInsertCategory::kEmojis}));

class FakeQuickInsertViewDelegate : public QuickInsertViewDelegate {
 public:
  using FakeSearchFunction =
      base::RepeatingCallback<void(std::u16string_view query,
                                   SearchResultsCallback callback)>;
  using FakeCategorySearchFunction =
      base::RepeatingCallback<void(SearchResultsCallback callback)>;

  struct Options {
    std::vector<QuickInsertCategory> available_categories;
    std::vector<QuickInsertSearchResult> zero_state_suggested_results;
    FakeSearchFunction search_function;
    base::RepeatingClosure stop_search_function;
    FakeCategorySearchFunction category_results_function;
    QuickInsertActionType action_type = QuickInsertActionType::kInsert;
    std::vector<QuickInsertEmojiResult> emoji_results;
    std::vector<std::string> suggested_emojis;
    PickerModeType mode = PickerModeType::kNoSelection;
  };

  FakeQuickInsertViewDelegate() = default;
  explicit FakeQuickInsertViewDelegate(Options options) : options_(options) {}

  std::vector<QuickInsertCategory> GetAvailableCategories() override {
    if (options_.available_categories.empty()) {
      // Use at least one category.
      return {QuickInsertCategory::kLinks};
    }
    return options_.available_categories;
  }

  void GetZeroStateSuggestedResults(
      SuggestedResultsCallback callback) override {
    callback.Run(options_.zero_state_suggested_results);
  }

  void GetResultsForCategory(QuickInsertCategory category,
                             SearchResultsCallback callback) override {
    if (options_.category_results_function.is_null()) {
      std::move(callback).Run({});
    } else {
      options_.category_results_function.Run(std::move(callback));
    }
  }

  void StartSearch(std::u16string_view query,
                   std::optional<QuickInsertCategory> category,
                   SearchResultsCallback callback) override {
    if (options_.search_function.is_null()) {
      std::move(callback).Run({});
    } else {
      options_.search_function.Run(query, std::move(callback));
    }
  }

  void StopSearch() override {
    if (!options_.stop_search_function.is_null()) {
      options_.stop_search_function.Run();
    }
  }

  void StartEmojiSearch(std::u16string_view query,
                        EmojiSearchResultsCallback callback) override {
    std::move(callback).Run(options_.emoji_results);
  }

  void CloseWidgetThenInsertResultOnNextFocus(
      const QuickInsertSearchResult& result) override {
    last_inserted_result_ = result;
    session_metrics_.SetOutcome(
        PickerSessionMetrics::SessionOutcome::kInsertedOrCopied);
  }
  void OpenResult(const QuickInsertSearchResult& result) override {
    last_opened_result_ = result;
  }

  void ShowEmojiPicker(ui::EmojiPickerCategory category,
                       std::u16string_view query) override {
    emoji_picker_category_ = category;
    emoji_picker_query_ = std::u16string(query);
  }
  void ShowEditor(std::optional<std::string> preset_query_id,
                  std::optional<std::string> freeform_text) override {
    showed_editor_ = true;
  }
  void ShowLobster(std::optional<std::string> freeform_text) override {
    showed_lobster_ = true;
  }

  PickerAssetFetcher* GetAssetFetcher() override { return &asset_fetcher_; }

  PickerSessionMetrics& GetSessionMetrics() override {
    return session_metrics_;
  }
  QuickInsertActionType GetActionForResult(
      const QuickInsertSearchResult& result) override {
    return options_.action_type;
  }

  std::vector<QuickInsertEmojiResult> GetSuggestedEmoji() override {
    std::vector<QuickInsertEmojiResult> results;
    for (const std::string& emoji : options_.suggested_emojis) {
      results.push_back(
          QuickInsertEmojiResult::Emoji(base::UTF8ToUTF16(emoji)));
    }
    return results;
  }

  bool IsGifsEnabled() override { return true; }
  PickerModeType GetMode() override { return options_.mode; }

  PickerCapsLockPosition GetCapsLockPosition() override {
    return PickerCapsLockPosition::kTop;
  }

  std::optional<QuickInsertSearchResult> last_inserted_result() const {
    return last_inserted_result_;
  }
  std::optional<QuickInsertSearchResult> last_opened_result() const {
    return last_opened_result_;
  }

  std::optional<ui::EmojiPickerCategory> emoji_picker_category() const {
    return emoji_picker_category_;
  }
  std::optional<std::u16string> emoji_picker_query() const {
    return emoji_picker_query_;
  }
  bool showed_editor() const { return showed_editor_; }

  // TODO: b/348279987 - Adds unit test once the Lobster entry point is added to
  // zero state.
  bool showed_lobster() const { return showed_lobster_; }

 private:
  Options options_;
  MockPickerAssetFetcher asset_fetcher_;
  PickerSessionMetrics session_metrics_;
  std::optional<QuickInsertSearchResult> last_inserted_result_;
  std::optional<QuickInsertSearchResult> last_opened_result_;
  std::optional<ui::EmojiPickerCategory> emoji_picker_category_;
  std::optional<std::u16string> emoji_picker_query_;
  bool showed_editor_ = false;
  bool showed_lobster_ = false;
};

QuickInsertView* GetQuickInsertViewFromWidget(views::Widget& widget) {
  return views::AsViewClass<QuickInsertView>(
      widget.non_client_view()->client_view()->children().front());
}

// Gets the first category item view that can be clicked to select a category.
QuickInsertItemView* GetFirstCategoryItemView(
    QuickInsertView* quick_insert_view) {
  return quick_insert_view->zero_state_view_for_testing()
      .category_section_views_for_testing()
      .begin()
      ->second->item_views_for_testing()[0];
}

TEST_P(QuickInsertViewEmojiTest,
       SizeIsLessThanMaxWhenNoContentWithoutEmojiBar) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {GetParam()},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);

  EXPECT_EQ(view->size().width(), kQuickInsertViewWidth);
  EXPECT_LT(view->size().height(), 300);
}

TEST_P(QuickInsertViewEmojiTest, SizeIsLessThanMaxWhenNoContentWithEmojiBar) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {GetParam()},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);

  EXPECT_EQ(view->size().width(), kQuickInsertViewWidth);
  EXPECT_LT(view->size().height(), 356);
}

TEST_F(QuickInsertViewTest, SizeIsMaxWhenLotsOfContentWithoutEmojiBar) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks},
      .zero_state_suggested_results = std::vector<QuickInsertSearchResult>(
          10, QuickInsertTextResult(u"abc")),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);

  EXPECT_EQ(view->size(), gfx::Size(kQuickInsertViewWidth, 300));
}

TEST_P(QuickInsertViewEmojiTest, SizeIsMaxWhenLotsOfContentWithEmojiBar) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {GetParam()},
      .zero_state_suggested_results = std::vector<QuickInsertSearchResult>(
          10, QuickInsertTextResult(u"abc")),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);

  EXPECT_EQ(view->size(), gfx::Size(kQuickInsertViewWidth, 356));
}

TEST_F(QuickInsertViewTest, ShowsZeroStateView) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);

  EXPECT_THAT(view->search_field_view_for_testing(),
              Property(&views::View::GetVisible, true));
  EXPECT_THAT(view->zero_state_view_for_testing(),
              Property(&views::View::GetVisible, true));
  EXPECT_THAT(view->search_results_view_for_testing(),
              Property(&views::View::GetVisible, false));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(QuickInsertViewTest, SearchPlaceholderMatchesUnfocusedMode) {
  FakeQuickInsertViewDelegate delegate({
      .mode = PickerModeType::kUnfocused,
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  EXPECT_EQ(quick_insert_view->search_field_view_for_testing()
                .textfield_for_testing()
                .GetPlaceholderText(),
            l10n_util::GetStringUTF16(
                IDS_PICKER_SEARCH_FIELD_NO_FOCUS_PLACEHOLDER_TEXT));
}

TEST_F(QuickInsertViewTest, SearchPlaceholderMatchesNoSelectionModeWithEditor) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kEditorWrite},
      .mode = PickerModeType::kNoSelection,
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  EXPECT_EQ(
      quick_insert_view->search_field_view_for_testing()
          .textfield_for_testing()
          .GetPlaceholderText(),
      l10n_util::GetStringUTF16(
          IDS_PICKER_SEARCH_FIELD_NO_SELECTION_WITH_EDITOR_PLACEHOLDER_TEXT));
}

TEST_F(QuickInsertViewTest,
       SearchPlaceholderMatchesNoSelectionModeWithoutEditor) {
  FakeQuickInsertViewDelegate delegate({
      .mode = PickerModeType::kNoSelection,
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  EXPECT_EQ(quick_insert_view->search_field_view_for_testing()
                .textfield_for_testing()
                .GetPlaceholderText(),
            l10n_util::GetStringUTF16(
                IDS_PICKER_SEARCH_FIELD_NO_SELECTION_PLACEHOLDER_TEXT));
}

TEST_F(QuickInsertViewTest,
       SearchPlaceholderMatchesHasSelectionModeWithEditor) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kEditorRewrite},
      .mode = PickerModeType::kHasSelection,
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  EXPECT_EQ(
      quick_insert_view->search_field_view_for_testing()
          .textfield_for_testing()
          .GetPlaceholderText(),
      l10n_util::GetStringUTF16(
          IDS_PICKER_SEARCH_FIELD_HAS_SELECTION_WITH_EDITOR_PLACEHOLDER_TEXT));
}

TEST_F(QuickInsertViewTest,
       SearchPlaceholderMatchesHasSelectionModeWithoutEditor) {
  FakeQuickInsertViewDelegate delegate({
      .mode = PickerModeType::kHasSelection,
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  EXPECT_EQ(quick_insert_view->search_field_view_for_testing()
                .textfield_for_testing()
                .GetPlaceholderText(),
            l10n_util::GetStringUTF16(
                IDS_PICKER_SEARCH_FIELD_HAS_SELECTION_PLACEHOLDER_TEXT));
}
#endif

TEST_F(QuickInsertViewTest,
       NonEmptySearchFieldContentsSwitchesToSearchResultsView) {
  FakeQuickInsertViewDelegate delegate;
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  EXPECT_THAT(view->zero_state_view_for_testing(),
              Property(&views::View::GetVisible, false));
  EXPECT_THAT(view->search_results_view_for_testing(),
              Property(&views::View::GetVisible, true));
}

TEST_F(QuickInsertViewTest, EmptySearchFieldContentsSwitchesToZeroStateView) {
  FakeQuickInsertViewDelegate delegate;
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);

  EXPECT_THAT(view->zero_state_view_for_testing(),
              Property(&views::View::GetVisible, true));
  EXPECT_THAT(view->search_results_view_for_testing(),
              Property(&views::View::GetVisible, false));
}

TEST_F(QuickInsertViewTest, LeftClickSearchResultInsertsResult) {
  {
    base::test::TestFuture<void> future;
    FakeQuickInsertViewDelegate delegate({
        .search_function = base::BindLambdaForTesting(
            [&](std::u16string_view query,
                FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
              future.SetValue();
              callback.Run({
                  QuickInsertSearchResultsSection(
                      QuickInsertSectionType::kClipboard,
                      {{QuickInsertTextResult(u"result")}},
                      /*has_more_results=*/false),
              });
            }),
        .action_type = QuickInsertActionType::kInsert,
    });
    auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
    widget->Show();
    QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
    PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
    ASSERT_TRUE(future.Wait());
    ASSERT_THAT(
        view->search_results_view_for_testing().section_views_for_testing(),
        Not(IsEmpty()));
    ASSERT_THAT(view->search_results_view_for_testing()
                    .section_views_for_testing()[0]
                    ->item_views_for_testing(),
                Not(IsEmpty()));

    QuickInsertItemView* result_view = view->search_results_view_for_testing()
                                           .section_views_for_testing()[0]
                                           ->item_views_for_testing()[0];
    ViewDrawnWaiter().Wait(result_view);
    LeftClickOn(result_view);

    EXPECT_EQ(delegate.last_opened_result(), std::nullopt);
    EXPECT_THAT(delegate.last_inserted_result(),
                Optional(QuickInsertTextResult(u"result")));
  }

  cros_events::Picker_FinishSession expected_event;
  expected_event
      .SetOutcome(cros_events::PickerSessionOutcome::INSERTED_OR_COPIED)
      .SetAction(cros_events::PickerAction::UNKNOWN)
      .SetResultSource(cros_events::PickerResultSource::UNKNOWN)
      .SetResultType(cros_events::PickerResultType::TEXT)
      .SetTotalEdits(1)
      .SetFinalQuerySize(1)
      .SetResultIndex(0);
  EXPECT_THAT(metrics_recorder_.GetEvents(), ContainsEvent(expected_event));
}

TEST_F(QuickInsertViewTest, LeftClickZeroStateSuggestedResultInsertsResult) {
  {
    base::test::TestFuture<void> future;
    FakeQuickInsertViewDelegate delegate({
        .available_categories = {QuickInsertCategory::kLinks},
        .zero_state_suggested_results = std::vector<QuickInsertSearchResult>(
            10, QuickInsertTextResult(u"abc")),
        .action_type = QuickInsertActionType::kInsert,
    });
    auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
    widget->Show();
    QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
    QuickInsertItemView* result_view = view->zero_state_view_for_testing()
                                           .primary_section_view_for_testing()
                                           ->item_views_for_testing()[0];
    ViewDrawnWaiter().Wait(result_view);
    LeftClickOn(result_view);

    EXPECT_EQ(delegate.last_opened_result(), std::nullopt);
    EXPECT_THAT(delegate.last_inserted_result(),
                Optional(QuickInsertTextResult(u"abc")));
  }

  cros_events::Picker_FinishSession expected_event;
  expected_event
      .SetOutcome(cros_events::PickerSessionOutcome::INSERTED_OR_COPIED)
      .SetAction(cros_events::PickerAction::UNKNOWN)
      .SetResultSource(cros_events::PickerResultSource::UNKNOWN)
      .SetResultType(cros_events::PickerResultType::TEXT)
      .SetTotalEdits(0)
      .SetFinalQuerySize(0)
      .SetResultIndex(-1);
  EXPECT_THAT(metrics_recorder_.GetEvents(), ContainsEvent(expected_event));
}

TEST_F(QuickInsertViewTest, LeftClickSearchResultOpensResult) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kLinks,
                    {QuickInsertBrowsingHistoryResult({}, u"a", {})},
                    /*has_more_results=*/false),
            });
          }),
      .action_type = QuickInsertActionType::kOpen,
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  ASSERT_THAT(
      view->search_results_view_for_testing().section_views_for_testing(),
      Not(IsEmpty()));
  ASSERT_THAT(view->search_results_view_for_testing()
                  .section_views_for_testing()[0]
                  ->item_views_for_testing(),
              Not(IsEmpty()));

  QuickInsertItemView* result_view = view->search_results_view_for_testing()
                                         .section_views_for_testing()[0]
                                         ->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(result_view);
  LeftClickOn(result_view);

  EXPECT_EQ(delegate.last_inserted_result(), std::nullopt);
  EXPECT_THAT(delegate.last_opened_result(),
              Optional(QuickInsertBrowsingHistoryResult({}, u"a", {})));
}

TEST_F(QuickInsertViewTest, SwitchesToCategoryView) {
  {
    FakeQuickInsertViewDelegate delegate({
        .available_categories = {QuickInsertCategory::kLinks},
    });
    auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
    widget->Show();

    QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
    views::View* category_item_view =
        GetFirstCategoryItemView(quick_insert_view);

    category_item_view->ScrollViewToVisible();
    ViewDrawnWaiter().Wait(category_item_view);
    LeftClickOn(category_item_view);

    EXPECT_TRUE(
        quick_insert_view->category_results_view_for_testing().GetVisible());
    EXPECT_FALSE(quick_insert_view->zero_state_view_for_testing().GetVisible());
    EXPECT_FALSE(
        quick_insert_view->search_results_view_for_testing().GetVisible());
  }

  cros_events::Picker_FinishSession expected_event;
  expected_event.SetOutcome(cros_events::PickerSessionOutcome::ABANDONED)
      .SetAction(cros_events::PickerAction::OPEN_LINKS)
      .SetResultSource(cros_events::PickerResultSource::UNKNOWN)
      .SetResultType(cros_events::PickerResultType::UNKNOWN)
      .SetTotalEdits(0)
      .SetFinalQuerySize(0)
      .SetResultIndex(-1);
  EXPECT_THAT(metrics_recorder_.GetEvents(), ContainsEvent(expected_event));
}

TEST_F(QuickInsertViewTest, ClickingCategoryResultsSwitchesToCategoryView) {
  base::test::TestFuture<void> search_called;
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks},
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            search_called.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kNone,
                    {{QuickInsertCategoryResult(QuickInsertCategory::kLinks)}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_called.Wait());

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_result_item_view =
      quick_insert_view->search_results_view_for_testing()
          .section_views_for_testing()[0]
          ->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(category_result_item_view);
  LeftClickOn(category_result_item_view);

  EXPECT_TRUE(
      quick_insert_view->category_results_view_for_testing().GetVisible());
  EXPECT_FALSE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  EXPECT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
}

TEST_F(QuickInsertViewTest,
       SelectingCategoryUpdatesSearchFieldPlaceholderText) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);

  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  EXPECT_EQ(quick_insert_view->search_field_view_for_testing()
                .textfield_for_testing()
                .GetPlaceholderText(),
            l10n_util::GetStringUTF16(
                IDS_PICKER_LINKS_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT));
}

TEST_F(QuickInsertViewTest, SelectingCategoryShowsBackButton) {
  FakeQuickInsertViewDelegate delegate;
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);
  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);

  LeftClickOn(category_item_view);

  EXPECT_TRUE(quick_insert_view->search_field_view_for_testing()
                  .back_button_for_testing()
                  .GetVisible());
}

TEST_F(QuickInsertViewTest, SearchingWithCategoryKeepsShowingBackButton) {
  FakeQuickInsertViewDelegate delegate;
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);
  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  // Type something into the search field.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  EXPECT_TRUE(quick_insert_view->search_field_view_for_testing()
                  .back_button_for_testing()
                  .GetVisible());
}

TEST_P(QuickInsertViewEmojiTest, SelectingCategoryHidesEmojiBar) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks, GetParam()},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);
  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);

  LeftClickOn(category_item_view);

  EXPECT_FALSE(quick_insert_view->emoji_bar_view_for_testing()->GetVisible());
}

TEST_P(QuickInsertViewEmojiTest,
       ReturningToZeroStateFromCategoryPageShowsEmojiBar) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks, GetParam()},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);
  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_BACK, ui::EF_NONE);

  EXPECT_TRUE(quick_insert_view->emoji_bar_view_for_testing()->GetVisible());
}

TEST_F(QuickInsertViewTest, SearchingWithCategorySwitchesToSearchResultsView) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  // Switch to category view.
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);

  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);
  // Type something into the search field.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  EXPECT_FALSE(
      quick_insert_view->category_results_view_for_testing().GetVisible());
  EXPECT_FALSE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  EXPECT_TRUE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
}

TEST_F(QuickInsertViewTest, EmptySearchFieldSwitchesBackToCategoryView) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  // Switch to category view.
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);

  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);
  // Type something into the search field.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  // Clear the search field.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);

  EXPECT_TRUE(
      quick_insert_view->category_results_view_for_testing().GetVisible());
  EXPECT_FALSE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  EXPECT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
}

TEST_F(QuickInsertViewTest, EmptySearchFieldSwitchesToCategoryViewFromSeeMore) {
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [](std::u16string_view query,
             FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            callback.Run({
                QuickInsertSearchResultsSection(QuickInsertSectionType::kLinks,
                                                {},
                                                /*has_more_results=*/true),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  // Type something into the search field.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  // See more results.
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* trailing_link =
      quick_insert_view->search_results_view_for_testing()
          .section_views_for_testing()[0]
          ->title_trailing_link_for_testing();
  ViewDrawnWaiter().Wait(trailing_link);
  LeftClickOn(trailing_link);
  // Clear the search field.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);

  EXPECT_TRUE(
      quick_insert_view->category_results_view_for_testing().GetVisible());
  EXPECT_FALSE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  EXPECT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
}

TEST_F(QuickInsertViewTest, CategoryViewFromSeeMoreHasResults) {
  FakeQuickInsertViewDelegate delegate(
      {.search_function = base::BindLambdaForTesting(
           [&](std::u16string_view query,
               FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
             callback.Run({
                 QuickInsertSearchResultsSection(QuickInsertSectionType::kLinks,
                                                 {},
                                                 /*has_more_results=*/true),
             });
           }),
       .category_results_function = base::BindLambdaForTesting(
           [&](FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
             callback.Run({
                 QuickInsertSearchResultsSection(
                     QuickInsertSectionType::kLinks,
                     {
                         QuickInsertTextResult(u"result"),
                     },
                     /*has_more_results=*/false),
             });
           })});
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  // Type something into the search field.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  // See more results.
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* trailing_link =
      quick_insert_view->search_results_view_for_testing()
          .section_views_for_testing()[0]
          ->title_trailing_link_for_testing();
  ViewDrawnWaiter().Wait(trailing_link);
  LeftClickOn(trailing_link);
  // Clear the search field.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);

  ASSERT_TRUE(
      quick_insert_view->category_results_view_for_testing().GetVisible());
  EXPECT_THAT(quick_insert_view->category_results_view_for_testing()
                  .section_views_for_testing(),
              ElementsAre(Pointee(Property(
                  "item views", &QuickInsertSectionView::item_views_for_testing,
                  ElementsAre(AsView<QuickInsertListItemView>(Property(
                      "primary text",
                      &QuickInsertListItemView::GetPrimaryTextForTesting,
                      u"result")))))));
}

TEST_F(QuickInsertViewTest, SearchingSpacesFromZeroStateDoesNotStartSearch) {
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            ADD_FAILURE()
                << "Search function was unexpectedly called with query "
                << query;
            // This should never be run - but if it is, immediately publish
            // to get results.
            callback.Run({{QuickInsertSearchResultsSection(
                QuickInsertSectionType::kClipboard,
                {{QuickInsertTextResult(u"result")}},
                /*has_more_results=*/false)}});
            // Signals that all results are done.
            callback.Run({});
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_SPACE, ui::EF_NONE);
  task_environment()->FastForwardBy(QuickInsertView::kClearResultsTimeout);
  EXPECT_TRUE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  EXPECT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_SPACE, ui::EF_NONE);
  task_environment()->FastForwardBy(QuickInsertView::kClearResultsTimeout);
  EXPECT_TRUE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  EXPECT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
}

TEST_F(QuickInsertViewTest, SearchTrimsLeftAndRightSpaces) {
  base::test::TestFuture<std::u16string> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            // This will crash if it is run multiple times.
            future.SetValue(std::u16string(query));
            callback.Run({{QuickInsertSearchResultsSection(
                QuickInsertSectionType::kClipboard,
                {{QuickInsertTextResult(u"result")}},
                /*has_more_results=*/false)}});
            // Signals that all results are done.
            callback.Run({});
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_SPACE, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_SPACE, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_SPACE, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_SPACE, ui::EF_NONE);
  // [....|]
  PressAndReleaseKey(ui::KeyboardCode::VKEY_LEFT, ui::EF_NONE);
  // [...|.]
  PressAndReleaseKey(ui::KeyboardCode::VKEY_LEFT, ui::EF_NONE);
  // [..|..]
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  // [..a|..]
  ASSERT_EQ(quick_insert_view->search_field_view_for_testing()
                .textfield_for_testing()
                .GetText(),
            u"  a  ");
  task_environment()->FastForwardBy(QuickInsertView::kClearResultsTimeout);
  EXPECT_FALSE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  EXPECT_TRUE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
  EXPECT_EQ(future.Take(), u"a");
}

TEST_F(QuickInsertViewTest, SearchIsNotRerunIfSpacesAreAddedToEnds) {
  base::test::TestFuture<std::u16string> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            // This will crash if it is run multiple times.
            future.SetValue(std::u16string(query));
            callback.Run({{QuickInsertSearchResultsSection(
                QuickInsertSectionType::kClipboard,
                {{QuickInsertTextResult(u"result")}},
                /*has_more_results=*/false)}});
            // Signals that all results are done.
            callback.Run({});
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(future.Get(), u"a");
  PressAndReleaseKey(ui::KeyboardCode::VKEY_SPACE, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_SPACE, ui::EF_NONE);
  // [a..|]
  PressAndReleaseKey(ui::KeyboardCode::VKEY_HOME, ui::EF_NONE);
  // [|a..]
  PressAndReleaseKey(ui::KeyboardCode::VKEY_SPACE, ui::EF_NONE);
  // [.|a..]
  PressAndReleaseKey(ui::KeyboardCode::VKEY_SPACE, ui::EF_NONE);
  // [..|a..]
  ASSERT_EQ(quick_insert_view->search_field_view_for_testing()
                .textfield_for_testing()
                .GetText(),
            u"  a  ");
}

TEST_F(QuickInsertViewTest,
       SearchingFromZeroStateDoesNotImmediatelySwitchToResults) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue(std::move(callback));
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  FakeQuickInsertViewDelegate::SearchResultsCallback callback = future.Take();

  EXPECT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
  callback.Run({{QuickInsertSearchResultsSection(
      QuickInsertSectionType::kClipboard, {{QuickInsertTextResult(u"result")}},
      /*has_more_results=*/false)}});
  EXPECT_TRUE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
}

TEST_F(QuickInsertViewTest,
       SearchingFromZeroStateSwitchesToEmptyResultsAfterTimeout) {
  base::test::TestFuture<void> search_called;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            search_called.SetValue();
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_called.Wait());

  EXPECT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
  task_environment()->FastForwardBy(QuickInsertView::kClearResultsTimeout);
  EXPECT_TRUE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(quick_insert_view->search_results_view_for_testing()
                  .section_list_view_for_testing()
                  ->children(),
              IsEmpty());
}

TEST_F(QuickInsertViewTest,
       SearchingFromCategoryDoesNotImmediatelySwitchToResults) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      future;
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks},
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue(std::move(callback));
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);

  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  ASSERT_TRUE(
      quick_insert_view->category_results_view_for_testing().GetVisible());
  ASSERT_FALSE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  ASSERT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  FakeQuickInsertViewDelegate::SearchResultsCallback callback = future.Take();

  EXPECT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
  callback.Run({{QuickInsertSearchResultsSection(
      QuickInsertSectionType::kLinks, {{QuickInsertTextResult(u"result")}},
      /*has_more_results=*/false)}});
  EXPECT_TRUE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
}

TEST_F(QuickInsertViewTest,
       SearchingFromCategorySwitchesToEmptyResultsAfterTimeout) {
  base::test::TestFuture<void> search_called;
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks},
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            search_called.SetValue();
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);

  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  ASSERT_TRUE(
      quick_insert_view->category_results_view_for_testing().GetVisible());
  ASSERT_FALSE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  ASSERT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_called.Wait());

  EXPECT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
  task_environment()->FastForwardBy(QuickInsertView::kClearResultsTimeout);
  EXPECT_TRUE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(quick_insert_view->search_results_view_for_testing()
                  .section_list_view_for_testing()
                  ->children(),
              IsEmpty());
}

TEST_F(QuickInsertViewTest,
       SearchingShowResultsWhenResultsArriveAsynchronously) {
  base::test::TestFuture<void> search_called;
  FakeQuickInsertViewDelegate::SearchResultsCallback search_callback;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            search_callback = std::move(callback);
            search_called.SetValue();
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_called.Wait());

  search_callback.Run({
      QuickInsertSearchResultsSection(QuickInsertSectionType::kLinks, {},
                                      /*has_more_results=*/false),
  });

  EXPECT_TRUE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(quick_insert_view->search_results_view_for_testing()
                  .section_views_for_testing(),
              ElementsAre(Pointee(Property(
                  "title", &QuickInsertSectionView::title_label_for_testing,
                  Property("text", &views::Label::GetText,
                           l10n_util::GetStringUTF16(
                               IDS_PICKER_LINKS_CATEGORY_LABEL))))));
}

TEST_F(QuickInsertViewTest, SearchingKeepsOldResultsUntilNewResultsArrive) {
  base::test::TestFuture<void> search1_called;
  base::test::TestFuture<void> search2_called;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            if (!search1_called.IsReady()) {
              callback.Run({
                  QuickInsertSearchResultsSection(
                      QuickInsertSectionType::kLinks, {},
                      /*has_more_results=*/false),
              });
              search1_called.SetValue();
            } else {
              search2_called.SetValue();
            }
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  // Go to the results page.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search1_called.Wait());
  // Start another search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search2_called.Wait());

  // Results page should keep old results until new results arrive.
  EXPECT_TRUE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(quick_insert_view->search_results_view_for_testing()
                  .section_views_for_testing(),
              ElementsAre(Pointee(Property(
                  "title", &QuickInsertSectionView::title_label_for_testing,
                  Property("text", &views::Label::GetText,
                           l10n_util::GetStringUTF16(
                               IDS_PICKER_LINKS_CATEGORY_LABEL))))));
}

TEST_F(QuickInsertViewTest, SearchingReplacesOldResultsWithNewResults) {
  base::test::TestFuture<void> search1_called;
  base::test::TestFuture<void> search2_called;
  FakeQuickInsertViewDelegate::SearchResultsCallback search2_callback;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            if (!search1_called.IsReady()) {
              callback.Run({
                  QuickInsertSearchResultsSection(
                      QuickInsertSectionType::kLocalFiles, {},
                      /*has_more_results=*/false),
              });
              search1_called.SetValue();
            } else {
              search2_callback = std::move(callback);
              search2_called.SetValue();
            }
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  // Go to the results page.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search1_called.Wait());
  // Start another search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search2_called.Wait());
  search2_callback.Run({
      QuickInsertSearchResultsSection(QuickInsertSectionType::kLinks, {},
                                      /*has_more_results=*/false),
  });

  // Results page should show the new results.
  EXPECT_TRUE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(quick_insert_view->search_results_view_for_testing()
                  .section_views_for_testing(),
              ElementsAre(Pointee(Property(
                  "title", &QuickInsertSectionView::title_label_for_testing,
                  Property("text", &views::Label::GetText,
                           l10n_util::GetStringUTF16(
                               IDS_PICKER_LINKS_CATEGORY_LABEL))))));
}

TEST_F(QuickInsertViewTest, ShowsNoResultsBeforeTimeout) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue(std::move(callback));
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  task_environment()->FastForwardBy(QuickInsertView::kClearResultsTimeout -
                                    base::Milliseconds(1));
  future.Take().Run({});

  EXPECT_TRUE(quick_insert_view->search_results_view_for_testing()
                  .no_results_view_for_testing()
                  ->GetVisible());
}

TEST_F(QuickInsertViewTest, ShowsNoResultsAfterTimeout) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue(std::move(callback));
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  task_environment()->FastForwardBy(QuickInsertView::kClearResultsTimeout);
  future.Take().Run({});

  EXPECT_TRUE(quick_insert_view->search_results_view_for_testing()
                  .no_results_view_for_testing()
                  ->GetVisible());
}

TEST_F(QuickInsertViewTest, ShowsNoResultsWithNoIllustration) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue(std::move(callback));
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  task_environment()->FastForwardBy(QuickInsertView::kClearResultsTimeout);
  future.Take().Run({});

  EXPECT_TRUE(quick_insert_view->search_results_view_for_testing()
                  .no_results_view_for_testing()
                  ->GetVisible());
  EXPECT_FALSE(quick_insert_view->search_results_view_for_testing()
                   .no_results_illustration_for_testing()
                   .GetVisible());
  EXPECT_EQ(quick_insert_view->search_results_view_for_testing()
                .no_results_label_for_testing()
                .GetText(),
            l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT));
}

TEST_F(QuickInsertViewTest, NoMainResultsAndNoEmojisIsAnnounced) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue(std::move(callback));
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  future.Take().Run({});

  EXPECT_EQ(
      quick_insert_view->search_results_view_for_testing().GetAccessibleName(),
      l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT));
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 1);
}

TEST_P(QuickInsertViewEmojiTest, NoMainResultsAndSomeEmojisIsAnnounced) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      future;
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {GetParam()},
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue(std::move(callback));
          }),
      .emoji_results = {QuickInsertEmojiResult::Emoji(u"😊"),
                        QuickInsertEmojiResult::Symbol(u"♬")},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  future.Take().Run({});

  EXPECT_EQ(
      quick_insert_view->search_results_view_for_testing().GetAccessibleName(),
      u"2 emojis. No other results.");
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 1);
}

TEST_F(QuickInsertViewTest, DoesNotClearResultsBeforeTimeout) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue(std::move(callback));
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  FakeQuickInsertViewDelegate::SearchResultsCallback first_callback =
      future.Take();
  first_callback.Run({{QuickInsertSearchResultsSection(
      QuickInsertSectionType::kClipboard, {{QuickInsertTextResult(u"result")}},
      /*has_more_results=*/false)}});
  task_environment()->FastForwardBy(QuickInsertView::kClearResultsTimeout);
  ASSERT_FALSE(quick_insert_view->search_results_view_for_testing()
                   .section_views_for_testing()
                   .empty());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  future.Clear();
  task_environment()->FastForwardBy(QuickInsertView::kClearResultsTimeout -
                                    base::Milliseconds(1));

  EXPECT_FALSE(quick_insert_view->search_results_view_for_testing()
                   .section_views_for_testing()
                   .empty());
}

TEST_F(QuickInsertViewTest, ClearsResultsAfterTimeout) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue(std::move(callback));
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  FakeQuickInsertViewDelegate::SearchResultsCallback first_callback =
      future.Take();
  first_callback.Run({{QuickInsertSearchResultsSection(
      QuickInsertSectionType::kClipboard, {{QuickInsertTextResult(u"result")}},
      /*has_more_results=*/false)}});
  task_environment()->FastForwardBy(QuickInsertView::kClearResultsTimeout);
  ASSERT_FALSE(quick_insert_view->search_results_view_for_testing()
                   .section_views_for_testing()
                   .empty());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  future.Clear();
  task_environment()->FastForwardBy(QuickInsertView::kClearResultsTimeout);

  EXPECT_TRUE(quick_insert_view->search_results_view_for_testing()
                  .section_views_for_testing()
                  .empty());
}

TEST_F(QuickInsertViewTest, ClearsResultsWhenQueryClearedNoCategory) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue(std::move(callback));
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  FakeQuickInsertViewDelegate::SearchResultsCallback callback = future.Take();
  callback.Run({{QuickInsertSearchResultsSection(
      QuickInsertSectionType::kClipboard, {{QuickInsertTextResult(u"result")}},
      /*has_more_results=*/false)}});
  ASSERT_FALSE(quick_insert_view->search_results_view_for_testing()
                   .section_views_for_testing()
                   .empty());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);

  EXPECT_TRUE(quick_insert_view->search_results_view_for_testing()
                  .section_views_for_testing()
                  .empty());
}

TEST_F(QuickInsertViewTest, ClearsResultsWhenQueryClearedWithCategory) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      future;
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks},
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue(std::move(callback));
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);

  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  ASSERT_TRUE(
      quick_insert_view->category_results_view_for_testing().GetVisible());
  ASSERT_FALSE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  ASSERT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  FakeQuickInsertViewDelegate::SearchResultsCallback callback = future.Take();
  callback.Run({{QuickInsertSearchResultsSection(
      QuickInsertSectionType::kLinks, {{QuickInsertTextResult(u"result")}},
      /*has_more_results=*/false)}});
  ASSERT_FALSE(quick_insert_view->search_results_view_for_testing()
                   .section_views_for_testing()
                   .empty());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_TRUE(quick_insert_view->search_results_view_for_testing()
                  .section_views_for_testing()
                  .empty());
  EXPECT_TRUE(
      quick_insert_view->category_results_view_for_testing().GetVisible());
  EXPECT_FALSE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  EXPECT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
}

TEST_F(QuickInsertViewTest, StopsSearchWhenQueryClearedNoCategory) {
  base::test::TestFuture<void> search_future;
  base::test::TestFuture<void> stop_search_future;
  FakeQuickInsertViewDelegate delegate(
      {.search_function = base::BindLambdaForTesting(
           [&](std::u16string_view query,
               FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
             search_future.SetValue();
           }),
       .stop_search_function = stop_search_future.GetRepeatingCallback()});
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_future.Wait());
  EXPECT_FALSE(stop_search_future.IsReady());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_TRUE(stop_search_future.Wait());
}

TEST_F(QuickInsertViewTest, StopsSearchWhenQueryClearedWithCategory) {
  base::test::TestFuture<void> search_future;
  base::test::TestFuture<void> stop_search_future;
  FakeQuickInsertViewDelegate delegate(
      {.available_categories = {QuickInsertCategory::kLinks},
       .search_function = base::BindLambdaForTesting(
           [&](std::u16string_view query,
               FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
             search_future.SetValue();
           }),
       .stop_search_function = stop_search_future.GetRepeatingCallback()});
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);

  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);
  // Starting a category search - even if there is no query - stops the previous
  // search.
  ASSERT_TRUE(stop_search_future.WaitAndClear());

  ASSERT_TRUE(
      quick_insert_view->category_results_view_for_testing().GetVisible());
  ASSERT_FALSE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  ASSERT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_future.Wait());
  EXPECT_FALSE(stop_search_future.IsReady());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_TRUE(stop_search_future.Wait());
  EXPECT_TRUE(
      quick_insert_view->category_results_view_for_testing().GetVisible());
  EXPECT_FALSE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  EXPECT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
}

TEST_F(QuickInsertViewTest, StopsSearchWhenBackButtonPressed) {
  base::test::TestFuture<void> search_future;
  base::test::TestFuture<void> stop_search_future;
  FakeQuickInsertViewDelegate delegate(
      {.available_categories = {QuickInsertCategory::kLinks},
       .search_function = base::BindLambdaForTesting(
           [&](std::u16string_view query,
               FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
             search_future.SetValue();
           }),
       .stop_search_function = stop_search_future.GetRepeatingCallback()});
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);

  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);
  // Starting a category search - even if there is no query - stops the previous
  // search.
  ASSERT_TRUE(stop_search_future.WaitAndClear());

  ASSERT_TRUE(
      quick_insert_view->category_results_view_for_testing().GetVisible());
  ASSERT_FALSE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  ASSERT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_future.Wait());
  ASSERT_FALSE(stop_search_future.IsReady());

  QuickInsertSearchFieldView& search_field_view =
      quick_insert_view->search_field_view_for_testing();
  ViewDrawnWaiter().Wait(&search_field_view.back_button_for_testing());
  LeftClickOn(&search_field_view.back_button_for_testing());

  EXPECT_TRUE(stop_search_future.Wait());
}

TEST_F(QuickInsertViewTest,
       StopsSearchWhenCategorySelectedOnZeroStateDuringSearch) {
  base::test::TestFuture<void> search_future;
  base::test::TestFuture<void> stop_search_future;
  FakeQuickInsertViewDelegate delegate(
      {.available_categories = {QuickInsertCategory::kLinks},
       .search_function = base::BindLambdaForTesting(
           [&](std::u16string_view query,
               FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
             search_future.SetValue();
           }),
       .stop_search_function = stop_search_future.GetRepeatingCallback()});
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_future.Wait());
  ASSERT_FALSE(stop_search_future.IsReady());

  ASSERT_FALSE(
      quick_insert_view->category_results_view_for_testing().GetVisible());
  ASSERT_TRUE(quick_insert_view->zero_state_view_for_testing().GetVisible());
  ASSERT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());

  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  EXPECT_TRUE(stop_search_future.Wait());
}

TEST_F(QuickInsertViewTest, StopsSearchWhenCategorySelectedInSearchResults) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      search_future;
  base::test::TestFuture<void> stop_search_future;
  FakeQuickInsertViewDelegate delegate(
      {.search_function = base::BindLambdaForTesting(
           [&](std::u16string_view query,
               FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
             search_future.SetValue(std::move(callback));
           }),
       .stop_search_function = stop_search_future.GetRepeatingCallback()});
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  FakeQuickInsertViewDelegate::SearchResultsCallback callback =
      search_future.Take();
  callback.Run({
      QuickInsertSearchResultsSection(
          QuickInsertSectionType::kNone,
          {{QuickInsertCategoryResult(QuickInsertCategory::kLinks)}},
          /*has_more_results=*/false),
  });

  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_result = view->search_results_view_for_testing()
                                     .section_list_view_for_testing()
                                     ->GetTopItem();
  ASSERT_TRUE(category_result);
  ViewDrawnWaiter().Wait(category_result);
  ASSERT_FALSE(stop_search_future.IsReady());
  LeftClickOn(category_result);

  ASSERT_TRUE(view->category_results_view_for_testing().GetVisible());
  ASSERT_FALSE(view->zero_state_view_for_testing().GetVisible());
  ASSERT_FALSE(view->search_results_view_for_testing().GetVisible());
  EXPECT_TRUE(stop_search_future.Wait());
}

TEST_P(QuickInsertViewEmojiTest, SearchingShowsExpressionResultsInEmojiBar) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {GetParam()},
      .emoji_results = {QuickInsertEmojiResult::Emoji(u"😊"),
                        QuickInsertEmojiResult::Symbol(u"♬")},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  ASSERT_NE(quick_insert_view->emoji_bar_view_for_testing(), nullptr);
  EXPECT_TRUE(quick_insert_view->emoji_bar_view_for_testing()->GetVisible());
  EXPECT_THAT(
      quick_insert_view->emoji_bar_view_for_testing()->GetItemsForTesting(),
      ElementsAre(Truly(&views::IsViewClass<PickerEmojiItemView>),
                  Truly(&views::IsViewClass<PickerEmojiItemView>)));
}

TEST_P(QuickInsertViewEmojiTest, InitiallyShowsSuggestedEmojis) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {GetParam()},
      .suggested_emojis = {"😊", "👍"},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  ASSERT_NE(quick_insert_view->emoji_bar_view_for_testing(), nullptr);
  EXPECT_TRUE(quick_insert_view->emoji_bar_view_for_testing()->GetVisible());
  EXPECT_THAT(
      quick_insert_view->emoji_bar_view_for_testing()->GetItemsForTesting(),
      ElementsAre(AsView<PickerEmojiItemView>(
                      Property(&PickerEmojiItemView::GetTextForTesting, u"😊")),
                  AsView<PickerEmojiItemView>(Property(
                      &PickerEmojiItemView::GetTextForTesting, u"👍"))));
}

TEST_F(QuickInsertViewTest, NoEmojiBarIfExpressionsCategoryNotAvailable) {
  FakeQuickInsertViewDelegate delegate(
      {.available_categories = {QuickInsertCategory::kLinks}});
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);

  EXPECT_EQ(GetQuickInsertViewFromWidget(*widget)->emoji_bar_view_for_testing(),
            nullptr);
}

TEST_F(QuickInsertViewTest, ClearsResultsWhenGoingBackToZeroState) {
  base::test::TestFuture<void> search_called;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            search_called.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kClipboard,
                    {{QuickInsertTextResult(u"result")}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  // Go to the results page.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(search_called.Wait());
  // Go back to the zero state page.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);

  EXPECT_FALSE(
      quick_insert_view->search_results_view_for_testing().GetVisible());
  EXPECT_THAT(quick_insert_view->search_results_view_for_testing()
                  .section_list_view_for_testing()
                  ->children(),
              IsEmpty());
}

TEST_F(QuickInsertViewTest, PressingEscClosesQuickInsertWidget) {
  FakeQuickInsertViewDelegate delegate;
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);

  EXPECT_TRUE(widget->IsClosed());
}

TEST_F(QuickInsertViewTest, RecordsSearchLatencyAfterSearchFinished) {
  base::HistogramTester histogram;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&, this](
              std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            // The search automatically publishes results after burn-in + 50ms,
            // so publish "burn in results" before that.
            task_environment()->FastForwardBy(
                QuickInsertController::kBurnInPeriod);
            // This needs to be non-empty, or else `MarkSearchResultsUpdated`
            // will be called with `kNoResultsFound` - which does not emit the
            // search latency metric.
            // TODO: b/349913604 - Replace the metric with a new one which
            // records search latency even if "no results found" was shown.
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kClipboard,
                    {{QuickInsertTextResult(u"result")}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.SearchLatency",
                                   QuickInsertController::kBurnInPeriod, 1);
}

TEST_F(QuickInsertViewTest,
       RecordsSearchLatencyWhenResultsAreAutomaticallyCleared) {
  base::HistogramTester histogram;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&, this](
              std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            task_environment()->FastForwardBy(
                QuickInsertView::kClearResultsTimeout);
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.SearchLatency",
                                   QuickInsertView::kClearResultsTimeout, 1);
}

TEST_F(QuickInsertViewTest, BoundsDefaultAlignedWithAnchor) {
  FakeQuickInsertViewDelegate delegate;
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
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

TEST_F(QuickInsertViewTest, BoundsAlignedWithAnchorNearTopLeftOfScreen) {
  FakeQuickInsertViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect anchor_bounds(screen_work_area.origin(), {0, 10});
  anchor_bounds.Offset(80, 120);

  auto widget = QuickInsertWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
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

TEST_F(QuickInsertViewTest, BoundsAlignedWithAnchorNearBottomLeftOfScreen) {
  FakeQuickInsertViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect anchor_bounds(screen_work_area.bottom_left(), {0, 10});
  anchor_bounds.Offset(80, -80);

  auto widget = QuickInsertWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
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

TEST_F(QuickInsertViewTest, BoundsBelowAnchorForAnchorNearTopRightOfScreen) {
  FakeQuickInsertViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect anchor_bounds(screen_work_area.top_right(), {0, 10});
  anchor_bounds.Offset(-20, 20);

  auto widget = QuickInsertWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  const QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be below the anchor.
  EXPECT_EQ(view->GetBoundsInScreen().y(), anchor_bounds.bottom());
}

TEST_F(QuickInsertViewTest, BoundsAboveAnchorForAnchorNearBottomRightOfScreen) {
  FakeQuickInsertViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect anchor_bounds(screen_work_area.bottom_right(), {0, 10});
  anchor_bounds.Offset(-20, -20);

  auto widget = QuickInsertWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  const QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  // Should be entirely on screen.
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  // Should be above the anchor.
  EXPECT_EQ(view->GetBoundsInScreen().bottom(), anchor_bounds.y());
}

TEST_F(QuickInsertViewTest, BoundsLeftAlignedBelowSelectionNearTopOfScreen) {
  FakeQuickInsertViewDelegate delegate({
      .mode = PickerModeType::kHasSelection,
  });
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  const gfx::Rect anchor_bounds(20, 20, 100, 20);

  auto widget = QuickInsertWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  const QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  EXPECT_EQ(view->GetBoundsInScreen().x(), anchor_bounds.x());
  EXPECT_GE(view->GetBoundsInScreen().y(), anchor_bounds.bottom());
}

TEST_F(QuickInsertViewTest, BoundsLeftAlignedAboveSelectionNearBottomOfScreen) {
  FakeQuickInsertViewDelegate delegate({
      .mode = PickerModeType::kHasSelection,
  });
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  const gfx::Rect anchor_bounds(20, screen_work_area.bottom() - 30, 100, 20);

  auto widget = QuickInsertWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  const QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  EXPECT_TRUE(screen_work_area.Contains(view->GetBoundsInScreen()));
  EXPECT_EQ(view->GetBoundsInScreen().x(), anchor_bounds.x());
  EXPECT_LE(view->GetBoundsInScreen().bottom(), anchor_bounds.y());
}

TEST_F(QuickInsertViewTest, BoundsOnScreenForEmptyAnchorBounds) {
  FakeQuickInsertViewDelegate delegate;
  auto widget = QuickInsertWidget::Create(&delegate, gfx::Rect());
  widget->Show();

  const QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  EXPECT_TRUE(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().Contains(
          view->GetBoundsInScreen()));
}

TEST_F(QuickInsertViewTest, MainContentBelowSearchFieldNearTopOfScreen) {
  FakeQuickInsertViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect anchor_bounds(screen_work_area.top_center(), {0, 10});
  anchor_bounds.Offset(0, 80);

  auto widget = QuickInsertWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  EXPECT_GE(view->zero_state_view_for_testing().GetBoundsInScreen().y(),
            view->search_field_view_for_testing().GetBoundsInScreen().bottom());
}

TEST_F(QuickInsertViewTest, MainContentAboveSearchFieldNearBottomOfScreen) {
  FakeQuickInsertViewDelegate delegate;
  const gfx::Rect screen_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect anchor_bounds(screen_work_area.bottom_center(), {0, 10});
  anchor_bounds.Offset(0, -80);

  auto widget = QuickInsertWidget::Create(&delegate, anchor_bounds);
  widget->Show();

  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  EXPECT_LE(view->zero_state_view_for_testing().GetBoundsInScreen().bottom(),
            view->search_field_view_for_testing().GetBoundsInScreen().y());
}

TEST_P(QuickInsertViewEmojiTest, ShowsEmojiPickerWhenClickingOnExpressions) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {GetParam()},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  LeftClickOn(GetFirstCategoryItemView(GetQuickInsertViewFromWidget(*widget)));

  EXPECT_TRUE(widget->IsClosed());
  EXPECT_THAT(delegate.emoji_picker_query(), Optional(Eq(u"")));
}

TEST_F(QuickInsertViewTest, ShowsEditorWhenClickingOnEditor) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kEditorWrite},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  LeftClickOn(GetFirstCategoryItemView(GetQuickInsertViewFromWidget(*widget)));

  EXPECT_TRUE(widget->IsClosed());
  EXPECT_TRUE(delegate.showed_editor());
}

TEST_F(QuickInsertViewTest, PressingEnterDoesNothingOnEmptySearchResultsPage) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(QuickInsertSectionType::kLinks,
                                                {},
                                                /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_TRUE(view->search_results_view_for_testing().GetVisible());
  EXPECT_EQ(delegate.last_inserted_result(), std::nullopt);
}

TEST_F(QuickInsertViewTest, PressingEnterDefaultSelectsFirstSearchResult) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kClipboard,
                    {{QuickInsertTextResult(u"Result A"),
                      QuickInsertTextResult(u"Result B")}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(QuickInsertTextResult(u"Result A")));
}

TEST_F(QuickInsertViewTest, ArrowKeysNavigateEmojiBar) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kEmojisGifs},
      .emoji_results = {QuickInsertEmojiResult::Emoji(u"😊"),
                        QuickInsertEmojiResult::Symbol(u"♬")},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ViewDrawnWaiter().Wait(GetQuickInsertViewFromWidget(*widget)
                             ->emoji_bar_view_for_testing()
                             ->GetTopItem());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_UP, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RIGHT, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(QuickInsertEmojiResult::Symbol(u"♬")));
}

TEST_F(QuickInsertViewTest, CanTypeQueryWhileEmojiBarIsPseudoFocused) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kEmojisGifs},
      .emoji_results = {QuickInsertEmojiResult::Emoji(u"😊"),
                        QuickInsertEmojiResult::Symbol(u"♬")},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ViewDrawnWaiter().Wait(GetQuickInsertViewFromWidget(*widget)
                             ->emoji_bar_view_for_testing()
                             ->GetTopItem());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_UP, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_B, ui::EF_NONE);

  EXPECT_EQ(GetQuickInsertViewFromWidget(*widget)
                ->search_field_view_for_testing()
                .textfield_for_testing()
                .GetText(),
            u"ab");
}

TEST_F(QuickInsertViewTest, DownArrowKeyNavigatesSearchResults) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kNone,
                    {{QuickInsertBrowsingHistoryResult(
                          GURL("http://foo.com"), u"Foo", ui::ImageModel()),
                      QuickInsertBrowsingHistoryResult(
                          GURL("http://bar.com"), u"Bar", ui::ImageModel())}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(QuickInsertBrowsingHistoryResult(
                  GURL("http://bar.com"), u"Bar", ui::ImageModel())));
}

TEST_F(QuickInsertViewTest, RightArrowKeyShowsSubmenu) {
  FakeQuickInsertViewDelegate delegate({
      .zero_state_suggested_results =
          {QuickInsertNewWindowResult(QuickInsertNewWindowResult::Type::kDoc),
           QuickInsertNewWindowResult(
               QuickInsertNewWindowResult::Type::kSheet)},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_RIGHT, ui::EF_NONE);

  EXPECT_NE(GetQuickInsertViewFromWidget(*widget)
                ->submenu_controller_for_testing()
                .GetSubmenuView(),
            nullptr);
}

TEST_F(QuickInsertViewTest, EnterKeyShowsSubmenu) {
  FakeQuickInsertViewDelegate delegate({
      .zero_state_suggested_results =
          {QuickInsertNewWindowResult(QuickInsertNewWindowResult::Type::kDoc),
           QuickInsertNewWindowResult(
               QuickInsertNewWindowResult::Type::kSheet)},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_NE(GetQuickInsertViewFromWidget(*widget)
                ->submenu_controller_for_testing()
                .GetSubmenuView(),
            nullptr);
}

TEST_F(QuickInsertViewTest, LeftArrowKeyClosesSubmenu) {
  FakeQuickInsertViewDelegate delegate({
      .zero_state_suggested_results =
          {QuickInsertNewWindowResult(QuickInsertNewWindowResult::Type::kDoc),
           QuickInsertNewWindowResult(
               QuickInsertNewWindowResult::Type::kSheet)},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  PressAndReleaseKey(ui::KeyboardCode::VKEY_RIGHT, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_LEFT, ui::EF_NONE);

  PickerSubmenuController& submenu_controller =
      GetQuickInsertViewFromWidget(*widget)->submenu_controller_for_testing();
  views::test::WidgetDestroyedWaiter(submenu_controller.widget_for_testing())
      .Wait();
  EXPECT_EQ(submenu_controller.GetSubmenuView(), nullptr);
}

TEST_F(QuickInsertViewTest, PressingEscClosesSubmenuThenWidget) {
  FakeQuickInsertViewDelegate delegate({
      .zero_state_suggested_results = {QuickInsertNewWindowResult(
          QuickInsertNewWindowResult::Type::kDoc)},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RIGHT, ui::EF_NONE);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);

  PickerSubmenuController& submenu_controller =
      GetQuickInsertViewFromWidget(*widget)->submenu_controller_for_testing();
  views::test::WidgetDestroyedWaiter(submenu_controller.widget_for_testing())
      .Wait();
  EXPECT_EQ(submenu_controller.GetSubmenuView(), nullptr);
  EXPECT_FALSE(widget->IsClosed());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);

  views::test::WidgetDestroyedWaiter(widget.get()).Wait();
}

TEST_F(QuickInsertViewTest, PressingEscClosesPreviewThenWidget) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kLocalFiles,
                    {{QuickInsertLocalFileResult(u"a", /*file_path=*/{})}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  PickerPreviewBubbleController& preview_controller =
      GetQuickInsertViewFromWidget(*widget)->preview_controller_for_testing();
  PickerPreviewBubbleVisibleWaiter().Wait(&preview_controller);
  EXPECT_TRUE(preview_controller.IsBubbleVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);

  EXPECT_FALSE(preview_controller.IsBubbleVisible());
  EXPECT_FALSE(widget->IsClosed());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);

  views::test::WidgetDestroyedWaiter(widget.get()).Wait();
}

TEST_F(QuickInsertViewTest, TabKeyNavigatesItemWithPreview) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kLocalFiles,
                    {{
                        QuickInsertTextResult(u"Result A"),
                        QuickInsertLocalFileResult(u"Result B",
                                                   /*file_path=*/{}),
                        QuickInsertTextResult(u"Result C"),
                    }},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  // Should navigate to the file result and show the preview bubble.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  PickerPreviewBubbleController& preview_controller =
      GetQuickInsertViewFromWidget(*widget)->preview_controller_for_testing();
  PickerPreviewBubbleVisibleWaiter().Wait(&preview_controller);

  EXPECT_TRUE(preview_controller.IsBubbleVisible());

  // Should close the preview bubble and navigate to the next result.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);

  EXPECT_FALSE(preview_controller.IsBubbleVisible());
  EXPECT_FALSE(widget->IsClosed());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(QuickInsertTextResult(u"Result C")));
}

TEST_F(QuickInsertViewTest, KeyEventsNavigateWithinSubmenu) {
  FakeQuickInsertViewDelegate delegate({
      .zero_state_suggested_results =
          {QuickInsertNewWindowResult(QuickInsertNewWindowResult::Type::kDoc),
           QuickInsertNewWindowResult(
               QuickInsertNewWindowResult::Type::kSheet)},
      .action_type = QuickInsertActionType::kOpen,
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  // Open submenu, navigate down to next submenu item, then select the item.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RIGHT, ui::EF_NONE);
  ViewDrawnWaiter().Wait(GetQuickInsertViewFromWidget(*widget)
                             ->submenu_controller_for_testing()
                             .GetSubmenuView());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_opened_result(),
              Optional(QuickInsertNewWindowResult(
                  QuickInsertNewWindowResult::Type::kSheet)));
}

TEST_F(QuickInsertViewTest, LeftArrowKeyNavigatesToBackButton) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  // Select a category so that the back button is visible.
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);
  category_item_view->ScrollViewToVisible();
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  // Navigate to the back button from the textfield, then select it.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_LEFT, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_TRUE(quick_insert_view->zero_state_view_for_testing().GetVisible());
}

TEST_F(QuickInsertViewTest, RightArrowKeyNavigatesToClearButton) {
  FakeQuickInsertViewDelegate delegate;
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  // Type a query so that the clear button is visible.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  const views::Textfield& textfield = GetQuickInsertViewFromWidget(*widget)
                                          ->search_field_view_for_testing()
                                          .textfield_for_testing();
  EXPECT_EQ(textfield.GetText(), u"a");

  // Navigate to the clear button from the textfield, then select it.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RIGHT, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_TRUE(textfield.GetText().empty());
}

TEST_F(QuickInsertViewTest, TabKeyNavigatesSearchResults) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kClipboard,
                    {{QuickInsertTextResult(u"Result A"),
                      QuickInsertTextResult(u"Result B")}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  ViewDrawnWaiter().Wait(GetQuickInsertViewFromWidget(*widget)
                             ->search_results_view_for_testing()
                             .section_list_view_for_testing()
                             ->GetTopItem());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(QuickInsertTextResult(u"Result B")));
}

TEST_F(QuickInsertViewTest, ShiftTabKeyNavigatesSearchResultsWithEmojiBar) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kEmojisGifs},
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kClipboard,
                    {{QuickInsertTextResult(u"Result A"),
                      QuickInsertTextResult(u"Result B")}},
                    /*has_more_results=*/false),
            });
          }),
      .emoji_results = {QuickInsertEmojiResult::Emoji(u"😊")},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  ViewDrawnWaiter().Wait(GetQuickInsertViewFromWidget(*widget)
                             ->search_results_view_for_testing()
                             .section_list_view_for_testing()
                             ->GetTopItem());

  // Navigate backward, to clear button.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  // Navigate backward, to textfield.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  // Navigate backward, to emoji bar.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  // Navigate backward, to the last search result.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_UP);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(QuickInsertTextResult(u"Result B")));
}

TEST_F(QuickInsertViewTest, ShiftTabKeyNavigatesSearchResultsWithoutEmojiBar) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kClipboard,
                    {{QuickInsertTextResult(u"Result A"),
                      QuickInsertTextResult(u"Result B")}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  ViewDrawnWaiter().Wait(GetQuickInsertViewFromWidget(*widget)
                             ->search_results_view_for_testing()
                             .section_list_view_for_testing()
                             ->GetTopItem());

  // Navigate backward, to clear button.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  // Navigate backward, to textfield.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  // Navigate backward, to the last search result.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(QuickInsertTextResult(u"Result B")));
}

TEST_F(QuickInsertViewTest, ShiftTabNavigatesToClearButton) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kClipboard,
                    {{QuickInsertTextResult(u"Result A"),
                      QuickInsertTextResult(u"Result B")}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  ViewDrawnWaiter().Wait(GetQuickInsertViewFromWidget(*widget)
                             ->search_results_view_for_testing()
                             .section_list_view_for_testing()
                             ->GetTopItem());

  // Navigate backward, to clear button.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_EQ(GetQuickInsertViewFromWidget(*widget)
                ->search_field_view_for_testing()
                .textfield_for_testing()
                .GetText(),
            u"");
}

TEST_F(QuickInsertViewTest,
       DownArrowKeyNavigatesFromClearButtonToSearchResults) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kClipboard,
                    {{QuickInsertTextResult(u"Result A"),
                      QuickInsertTextResult(u"Result B")}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  ViewDrawnWaiter().Wait(GetQuickInsertViewFromWidget(*widget)
                             ->search_results_view_for_testing()
                             .section_list_view_for_testing()
                             ->GetTopItem());

  // Navigate backward, to clear button.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  // Navigate downward, to the first search result.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(QuickInsertTextResult(u"Result A")));
}

TEST_F(QuickInsertViewTest, ShowsSubmenuOnMouseHover) {
  FakeQuickInsertViewDelegate delegate({
      .zero_state_suggested_results =
          {QuickInsertNewWindowResult(QuickInsertNewWindowResult::Type::kDoc),
           QuickInsertNewWindowResult(
               QuickInsertNewWindowResult::Type::kSheet)},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  GetEventGenerator()->MoveMouseTo(
      quick_insert_view->zero_state_view_for_testing()
          .primary_section_view_for_testing()
          ->item_views_for_testing()[0]
          ->GetBoundsInScreen()
          .CenterPoint());

  PickerSubmenuController& submenu_controller =
      quick_insert_view->submenu_controller_for_testing();
  views::test::WidgetVisibleWaiter(submenu_controller.widget_for_testing())
      .Wait();
  EXPECT_NE(submenu_controller.GetSubmenuView(), nullptr);
}

// This is an edge case where the user can open a submenu with mouse hover while
// they are using keyboard to navigate the main QuickInsertView. Since the
// keyboard selection can be separate to the mouse hover selection, we just
// close the submenu if the user resumes keyboard navigation in the main
// QuickInsertView.
TEST_F(QuickInsertViewTest,
       ClosesSubmenuWhenResumingKeyboardNavigationInMainView) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kEmojisGifs},
      .zero_state_suggested_results =
          {QuickInsertNewWindowResult(QuickInsertNewWindowResult::Type::kDoc),
           QuickInsertNewWindowResult(
               QuickInsertNewWindowResult::Type::kSheet)},
      .emoji_results = {QuickInsertEmojiResult::Emoji(u"😊"),
                        QuickInsertEmojiResult::Symbol(u"♬")},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();

  // Start keyboard navigation.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_UP, ui::EF_NONE);
  // Mouse hover over an item with a submenu to show a submenu.
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  GetEventGenerator()->MoveMouseTo(
      quick_insert_view->zero_state_view_for_testing()
          .primary_section_view_for_testing()
          ->item_views_for_testing()[0]
          ->GetBoundsInScreen()
          .CenterPoint());
  PickerSubmenuController& submenu_controller =
      quick_insert_view->submenu_controller_for_testing();
  views::test::WidgetVisibleWaiter(submenu_controller.widget_for_testing())
      .Wait();
  // Resume keyboard navigation.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RIGHT, ui::EF_NONE);

  views::test::WidgetDestroyedWaiter(submenu_controller.widget_for_testing())
      .Wait();
  EXPECT_EQ(submenu_controller.GetSubmenuView(), nullptr);
}

TEST_F(QuickInsertViewTest, ClearsSearchWhenClickingOnCategoryResult) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kNone,
                    {{QuickInsertCategoryResult(QuickInsertCategory::kLinks)}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_result = view->search_results_view_for_testing()
                                     .section_list_view_for_testing()
                                     ->GetTopItem();
  ASSERT_TRUE(category_result);
  ViewDrawnWaiter().Wait(category_result);

  LeftClickOn(category_result);

  EXPECT_EQ(
      view->search_field_view_for_testing().textfield_for_testing().GetText(),
      u"");
}

TEST_F(QuickInsertViewTest,
       PerformsCategorySearchWhenClickingOnSeeMoreResults) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(QuickInsertSectionType::kLinks,
                                                {},
                                                /*has_more_results=*/true),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  future.Clear();
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  views::View* trailing_link = view->search_results_view_for_testing()
                                   .section_views_for_testing()[0]
                                   ->title_trailing_link_for_testing();

  ViewDrawnWaiter().Wait(trailing_link);
  LeftClickOn(trailing_link);

  // Should call search a second time.
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(view->search_results_view_for_testing().GetVisible());
}

TEST_F(QuickInsertViewTest, KeyNavigationToSeeMoreResults) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kClipboard,
                    {{QuickInsertTextResult(u"Result A")}},
                    /*has_more_results=*/false),
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kLinks,
                    {QuickInsertBrowsingHistoryResult({}, u"Result B", {})},
                    /*has_more_results=*/true),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  future.Clear();
  ViewDrawnWaiter().Wait(GetQuickInsertViewFromWidget(*widget)
                             ->search_results_view_for_testing()
                             .section_list_view_for_testing()
                             ->GetTopItem());

  // Navigate to see more button, then press enter.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  // Should call search a second time.
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(GetQuickInsertViewFromWidget(*widget)
                  ->search_results_view_for_testing()
                  .GetVisible());
}

TEST_P(QuickInsertViewEmojiTest,
       ClickingMoreEmojisButtonOpensEmojiPickerWithQuerySearch) {
  FakeQuickInsertViewDelegate delegate({.available_categories = {GetParam()}});
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  PickerEmojiBarView* emoji_bar =
      GetQuickInsertViewFromWidget(*widget)->emoji_bar_view_for_testing();
  ASSERT_NE(emoji_bar, nullptr);
  views::View* more_emojis_button = emoji_bar->more_emojis_button_for_testing();
  ViewDrawnWaiter().Wait(more_emojis_button);
  LeftClickOn(more_emojis_button);

  EXPECT_TRUE(widget->IsClosed());
  EXPECT_THAT(delegate.emoji_picker_category(),
              Optional(Eq(ui::EmojiPickerCategory::kEmojis)));
  EXPECT_THAT(delegate.emoji_picker_query(), Optional(Eq(u"a")));
}

TEST_F(QuickInsertViewTest, ClickingGifsButtonOpensGifPickerWithQuerySearch) {
  FakeQuickInsertViewDelegate delegate(
      {.available_categories = {QuickInsertCategory::kEmojisGifs}});
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  PickerEmojiBarView* emoji_bar =
      GetQuickInsertViewFromWidget(*widget)->emoji_bar_view_for_testing();
  ASSERT_NE(emoji_bar, nullptr);
  views::View* gifs_button = emoji_bar->gifs_button_for_testing();
  ViewDrawnWaiter().Wait(gifs_button);
  LeftClickOn(gifs_button);

  EXPECT_TRUE(widget->IsClosed());
  EXPECT_THAT(delegate.emoji_picker_category(),
              Optional(Eq(ui::EmojiPickerCategory::kGifs)));
  EXPECT_THAT(delegate.emoji_picker_query(), Optional(Eq(u"a")));
}

TEST_F(QuickInsertViewTest,
       KeepsSearchFieldQueryTextAndFocusWhenClickingOnSeeMoreResults) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(QuickInsertSectionType::kLinks,
                                                {},
                                                /*has_more_results=*/true),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  future.Clear();
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
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

TEST_F(QuickInsertViewTest,
       CategoryOnlySearchShowsNoResultsPageWithNoIllustration) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks},
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({});
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(quick_insert_view->search_results_view_for_testing()
                  .no_results_view_for_testing()
                  ->GetVisible());
  EXPECT_FALSE(quick_insert_view->search_results_view_for_testing()
                   .no_results_illustration_for_testing()
                   .GetVisible());
  EXPECT_EQ(quick_insert_view->search_results_view_for_testing()
                .no_results_label_for_testing()
                .GetText(),
            l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT));
}

TEST_F(QuickInsertViewTest,
       CategoryZeroStateShowsNoResultsPageWithIllustration) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kLinks},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_item_view = GetFirstCategoryItemView(quick_insert_view);
  ViewDrawnWaiter().Wait(category_item_view);
  LeftClickOn(category_item_view);

  EXPECT_TRUE(quick_insert_view->category_results_view_for_testing()
                  .no_results_view_for_testing()
                  ->GetVisible());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(quick_insert_view->category_results_view_for_testing()
                  .no_results_illustration_for_testing()
                  .GetVisible());
#endif
  EXPECT_EQ(quick_insert_view->category_results_view_for_testing()
                .no_results_label_for_testing()
                .GetText(),
            l10n_util::GetStringUTF16(
                IDS_PICKER_NO_RESULTS_FOR_BROWSING_HISTORY_LABEL_TEXT));
}

TEST_F(
    QuickInsertViewTest,
    ChangingPseudoFocusOnZeroStateNotifiesInitialActiveDescendantChangeAfterDelay) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kClipboard,
                               QuickInsertCategory::kLinks},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  views::test::AXEventCounter counter(views::AXEventManager::Get());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);

  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kActiveDescendantChanged), 0);

  task_environment()->FastForwardBy(
      QuickInsertSearchFieldView::kNotifyInitialActiveDescendantA11yDelay);

  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kActiveDescendantChanged), 1);
}

TEST_F(
    QuickInsertViewTest,
    ChangingPseudoFocusOnZeroStateNotifiesActiveDescendantChangeImmediately) {
  FakeQuickInsertViewDelegate delegate({
      .available_categories = {QuickInsertCategory::kClipboard,
                               QuickInsertCategory::kLinks},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  views::test::AXEventCounter counter(views::AXEventManager::Get());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);
  task_environment()->FastForwardBy(
      QuickInsertSearchFieldView::kNotifyInitialActiveDescendantA11yDelay);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN, ui::EF_NONE);

  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kActiveDescendantChanged), 2);
}

TEST_F(QuickInsertViewTest, EnterOnZeroState) {
  FakeQuickInsertViewDelegate delegate({
      .zero_state_suggested_results = {QuickInsertTextResult(u"zero state")},
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  base::span<const raw_ptr<QuickInsertItemView>> zero_state_item_views =
      quick_insert_view->zero_state_view_for_testing()
          .primary_section_view_for_testing()
          ->item_views_for_testing();
  QuickInsertListItemView* suggested_item_view;
  ASSERT_THAT(
      zero_state_item_views,
      ElementsAre(ResultOf(
          [&](const raw_ptr<QuickInsertItemView> view) {
            ViewDrawnWaiter().Wait(view);
            suggested_item_view =
                views::AsViewClass<QuickInsertListItemView>(view);
            return suggested_item_view;
          },
          Pointee(
              AllOf(Property("primary text",
                             &QuickInsertListItemView::GetPrimaryTextForTesting,
                             u"zero state"),
                    Property("is visible", &views::View::GetVisible, true))))));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(VariantWith<QuickInsertTextResult>(
                  Field("primary text", &QuickInsertTextResult::primary_text,
                        u"zero state"))));
}

// TODO: b/351920494 - Insert the first new result instead of doing nothing.
TEST_F(QuickInsertViewTest, EnterDuringBurnInOnZeroState) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .zero_state_suggested_results = {QuickInsertTextResult(u"zero state")},
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  base::span<const raw_ptr<QuickInsertItemView>> zero_state_item_views =
      quick_insert_view->zero_state_view_for_testing()
          .primary_section_view_for_testing()
          ->item_views_for_testing();
  QuickInsertListItemView* suggested_item_view;
  ASSERT_THAT(
      zero_state_item_views,
      ElementsAre(ResultOf(
          [&](const raw_ptr<QuickInsertItemView> view) {
            ViewDrawnWaiter().Wait(view);
            suggested_item_view =
                views::AsViewClass<QuickInsertListItemView>(view);
            return suggested_item_view;
          },
          Pointee(
              AllOf(Property("primary text",
                             &QuickInsertListItemView::GetPrimaryTextForTesting,
                             u"zero state"),
                    Property("is visible", &views::View::GetVisible, true))))));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  // The suggested item should still be visible.
  ASSERT_TRUE(suggested_item_view->GetVisible());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_EQ(delegate.last_inserted_result(), std::nullopt);
}

TEST_F(QuickInsertViewTest, EnterOnSearchResults) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue(callback);
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  FakeQuickInsertViewDelegate::SearchResultsCallback first_callback =
      future.Take();
  first_callback.Run(
      {QuickInsertSearchResultsSection(QuickInsertSectionType::kClipboard,
                                       {QuickInsertTextResult(u"first search")},
                                       /*has_more_results=*/false)});
  base::span<const raw_ptr<QuickInsertSectionView>> section_views =
      quick_insert_view->search_results_view_for_testing()
          .section_views_for_testing();
  QuickInsertListItemView* search_item_view;
  ASSERT_THAT(
      section_views,
      ElementsAre(Pointee(Property(
          "item views", &QuickInsertSectionView::item_views_for_testing,
          ElementsAre(ResultOf(
              [&](const raw_ptr<QuickInsertItemView> view) {
                ViewDrawnWaiter().Wait(view);
                search_item_view =
                    views::AsViewClass<QuickInsertListItemView>(view);
                return search_item_view;
              },
              Pointee(AllOf(
                  Property("primary text",
                           &QuickInsertListItemView::GetPrimaryTextForTesting,
                           u"first search"),
                  Property("is visible", &views::View::GetVisible,
                           true)))))))));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_THAT(delegate.last_inserted_result(),
              Optional(VariantWith<QuickInsertTextResult>(
                  Field("primary text", &QuickInsertTextResult::primary_text,
                        u"first search"))));
}

// TODO: b/351920494 - Insert the first new result instead of doing nothing.
TEST_F(QuickInsertViewTest, EnterDuringBurnInOnSearchResults) {
  base::test::TestFuture<FakeQuickInsertViewDelegate::SearchResultsCallback>
      future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue(callback);
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  QuickInsertView* quick_insert_view = GetQuickInsertViewFromWidget(*widget);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  FakeQuickInsertViewDelegate::SearchResultsCallback first_callback =
      future.Take();
  first_callback.Run(
      {QuickInsertSearchResultsSection(QuickInsertSectionType::kClipboard,
                                       {QuickInsertTextResult(u"first search")},
                                       /*has_more_results=*/false)});
  base::span<const raw_ptr<QuickInsertSectionView>> section_views =
      quick_insert_view->search_results_view_for_testing()
          .section_views_for_testing();
  QuickInsertListItemView* search_item_view;
  ASSERT_THAT(
      section_views,
      ElementsAre(Pointee(Property(
          "item views", &QuickInsertSectionView::item_views_for_testing,
          ElementsAre(ResultOf(
              [&](const raw_ptr<QuickInsertItemView> view) {
                ViewDrawnWaiter().Wait(view);
                search_item_view =
                    views::AsViewClass<QuickInsertListItemView>(view);
                return search_item_view;
              },
              Pointee(AllOf(
                  Property("primary text",
                           &QuickInsertListItemView::GetPrimaryTextForTesting,
                           u"first search"),
                  Property("is visible", &views::View::GetVisible,
                           true)))))))));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  FakeQuickInsertViewDelegate::SearchResultsCallback second_callback =
      future.Take();
  // The search item should still be visible.
  ASSERT_TRUE(search_item_view->GetVisible());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);

  EXPECT_EQ(delegate.last_inserted_result(), std::nullopt);
}

TEST_F(QuickInsertViewTest, ResetsToZeroStateWhenClickingOnBackButton) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kNone,
                    {{QuickInsertCategoryResult(QuickInsertCategory::kLinks)}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_result = view->search_results_view_for_testing()
                                     .section_list_view_for_testing()
                                     ->GetTopItem();
  ASSERT_TRUE(category_result);
  ViewDrawnWaiter().Wait(category_result);
  LeftClickOn(category_result);

  QuickInsertSearchFieldView& search_field_view =
      view->search_field_view_for_testing();
  ViewDrawnWaiter().Wait(&search_field_view.back_button_for_testing());
  LeftClickOn(&search_field_view.back_button_for_testing());

  EXPECT_TRUE(view->zero_state_view_for_testing().GetVisible());
  EXPECT_EQ(search_field_view.textfield_for_testing().GetText(), u"");
  EXPECT_FALSE(search_field_view.clear_button_for_testing().GetVisible());
}

TEST_F(QuickInsertViewTest, ResetsToZeroStateAfterPressingBrowserBack) {
  base::test::TestFuture<void> future;
  FakeQuickInsertViewDelegate delegate({
      .search_function = base::BindLambdaForTesting(
          [&](std::u16string_view query,
              FakeQuickInsertViewDelegate::SearchResultsCallback callback) {
            future.SetValue();
            callback.Run({
                QuickInsertSearchResultsSection(
                    QuickInsertSectionType::kNone,
                    {{QuickInsertCategoryResult(QuickInsertCategory::kLinks)}},
                    /*has_more_results=*/false),
            });
          }),
  });
  auto widget = QuickInsertWidget::Create(&delegate, kDefaultAnchorBounds);
  widget->Show();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  ASSERT_TRUE(future.Wait());
  QuickInsertView* view = GetQuickInsertViewFromWidget(*widget);
  views::View* category_result = view->search_results_view_for_testing()
                                     .section_list_view_for_testing()
                                     ->GetTopItem();
  ASSERT_TRUE(category_result);
  ViewDrawnWaiter().Wait(category_result);
  LeftClickOn(category_result);

  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_BACK, ui::EF_NONE);

  EXPECT_TRUE(view->zero_state_view_for_testing().GetVisible());
  EXPECT_EQ(
      view->search_field_view_for_testing().textfield_for_testing().GetText(),
      u"");
  EXPECT_FALSE(view->search_field_view_for_testing()
                   .clear_button_for_testing()
                   .GetVisible());
}

}  // namespace
}  // namespace ash
