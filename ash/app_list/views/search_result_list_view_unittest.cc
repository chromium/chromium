// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_list_view.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_test_view_delegate.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/style/ash_color_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace test {

namespace {
int kDefaultSearchItems = 3;

// Preferred sizing for different types of search result views.
constexpr int kPreferredWidth = 640;
constexpr int kDefaultViewHeight = 40;
constexpr int kInlineAnswerViewHeight = 88;
constexpr gfx::Insets kInlineAnswerBorder(16);

// SearchResultListType::SearchResultListType::AnswerCard, and
//  SearchResultListType::kBestMatch do not have associated categories.
constexpr int num_list_types_not_in_category = 2;
// SearchResult::Category::kUnknown does not have an associated list type.
constexpr int num_category_without_list_type = 1;

}  // namespace

class SearchResultListViewTest : public views::test::WidgetTest {
 public:
  SearchResultListViewTest() = default;

  SearchResultListViewTest(const SearchResultListViewTest&) = delete;
  SearchResultListViewTest& operator=(const SearchResultListViewTest&) = delete;

  ~SearchResultListViewTest() override = default;

  // Overridden from testing::Test:
  void SetUp() override {
    views::test::WidgetTest::SetUp();
    widget_ = CreateTopLevelPlatformWidget();

    default_view_ = std::make_unique<SearchResultListView>(
        &view_delegate_, nullptr,
        SearchResultView::SearchResultViewType::kDefault, std::nullopt);
    default_view_->SetListType(
        SearchResultListView::SearchResultListType::kBestMatch);
    default_view_->SetActive(true);

    answer_card_view_ = std::make_unique<SearchResultListView>(
        &view_delegate_, nullptr,
        SearchResultView::SearchResultViewType::kAnswerCard, std::nullopt);
    answer_card_view_->SetListType(
        SearchResultListView::SearchResultListType::kAnswerCard);
    answer_card_view_->SetActive(true);

    widget_->SetBounds(gfx::Rect(0, 0, 700, 500));
    widget_->GetContentsView()->AddChildView(default_view_.get());
    widget_->GetContentsView()->AddChildView(answer_card_view_.get());
    widget_->Show();
    default_view_->SetResults(GetResults());
    answer_card_view_->SetResults(GetResults());
  }

  void TearDown() override {
    default_view_.reset();
    answer_card_view_.reset();
    widget_->CloseNow();
    views::test::WidgetTest::TearDown();
  }

 protected:
  SearchResultListView* default_view() const { return default_view_.get(); }
  SearchResultListView* answer_card_view() const {
    return answer_card_view_.get();
  }

  SearchResultView* GetDefaultResultViewAt(int index) const {
    return default_view_->GetResultViewAt(index);
  }
  SearchResultView* GetAnswerCardResultViewAt(int index) const {
    return answer_card_view_->GetResultViewAt(index);
  }

  views::FlexLayoutView* GetKeyboardShortcutContents(
      SearchResultView* result_view) {
    return result_view->get_keyboard_shortcut_container_for_test();
  }

  views::FlexLayoutView* GetTitleContents(SearchResultView* result_view) {
    return result_view->get_title_container_for_test();
  }

  views::FlexLayoutView* GetProgressBarContents(SearchResultView* result_view) {
    return result_view->get_progress_bar_container_for_test();
  }

  views::FlexLayoutView* GetDetailsContents(SearchResultView* result_view) {
    return result_view->get_details_container_for_test();
  }

  views::Label* GetResultTextSeparatorLabel(SearchResultView* result_view) {
    return result_view->get_result_text_separator_label_for_test();
  }

  std::vector<SearchResultView*> GetAssistantResultViews() const {
    std::vector<SearchResultView*> results;
    for (ash::SearchResultView* view : default_view_->search_result_views_) {
      auto* result = view->result();
      if (result &&
          result->result_type() == AppListSearchResultType::kAssistantText)
        results.push_back(view);
    }
    return results;
  }

  SearchModel::SearchResults* GetResults() {
    return AppListModelProvider::Get()->search_model()->results();
  }

  void AddAssistantSearchResult() {
    SearchModel::SearchResults* results = GetResults();

    std::unique_ptr<TestSearchResult> assistant_result =
        std::make_unique<TestSearchResult>();
    assistant_result->set_result_type(
        ash::AppListSearchResultType::kAssistantText);
    assistant_result->set_display_type(ash::SearchResultDisplayType::kList);
    assistant_result->SetAccessibleName(u"Accessible Name");
    assistant_result->SetTitle(u"assistant result");
    results->Add(std::move(assistant_result));

    RunPendingMessages();
  }

  void SetUpSearchResults() {
    SearchModel::SearchResults* results = GetResults();
    for (int i = 0; i < kDefaultSearchItems; ++i) {
      std::unique_ptr<TestSearchResult> result =
          std::make_unique<TestSearchResult>();
      result->set_display_type(ash::SearchResultDisplayType::kList);
      result->SetAccessibleName(
          base::UTF8ToUTF16(base::StringPrintf("Result %d", i)));
      result->SetTitle(base::UTF8ToUTF16(base::StringPrintf("Result %d", i)));
      result->set_best_match(true);
      if (i < 2) {
        result->SetAccessibleName(
            base::UTF8ToUTF16(base::StringPrintf("Result %d, Detail", i)));
        result->SetDetails(u"Detail");
      }
      results->Add(std::move(result));
    }

    // Adding results will schedule Update().
    RunPendingMessages();
  }

  std::vector<SearchResult::TextItem> BuildKeyboardShortcutTextVector() {
    std::vector<SearchResult::TextItem> keyboard_shortcut_text_vector;
    SearchResult::TextItem shortcut_text_item_1(
        ash::SearchResultTextItemType::kIconifiedText);
    shortcut_text_item_1.SetText(u"ctrl");
    shortcut_text_item_1.SetTextTags({});
    keyboard_shortcut_text_vector.push_back(shortcut_text_item_1);

    SearchResult::TextItem shortcut_text_item_2(
        ash::SearchResultTextItemType::kString);
    shortcut_text_item_2.SetText(u" + ");
    shortcut_text_item_2.SetTextTags({});
    keyboard_shortcut_text_vector.push_back(shortcut_text_item_2);

    SearchResult::TextItem shortcut_text_item_3(
        ash::SearchResultTextItemType::kIconifiedText);
    shortcut_text_item_3.SetText(u"a");
    shortcut_text_item_3.SetTextTags({});
    keyboard_shortcut_text_vector.push_back(shortcut_text_item_3);

    return keyboard_shortcut_text_vector;
  }

  void SetUpKeyboardShortcutResult() {
    SearchModel::SearchResults* results = GetResults();

    std::unique_ptr<TestSearchResult> result =
        std::make_unique<TestSearchResult>();
    result->set_display_type(ash::SearchResultDisplayType::kList);
    result->SetAccessibleName(u"Copy and Paste");
    result->SetTitle(u"Copy and Paste");
    result->SetDetails(u"Shortcuts");
    result->set_best_match(true);
    result->SetKeyboardShortcutTextVector(BuildKeyboardShortcutTextVector());
    results->Add(std::move(result));

    // Adding results will schedule Update().
    RunPendingMessages();
  }

  void SetUpKeyboardShortcutAnswerCard(bool long_title) {
    SearchModel::SearchResults* results = GetResults();
    std::unique_ptr<TestSearchResult> result =
        std::make_unique<TestSearchResult>();
    result->set_display_type(ash::SearchResultDisplayType::kAnswerCard);
    result->SetMultilineTitle(true);

    std::u16string title =
        long_title ? u"Arbitarily long answer card text to check multiline "
                     u"behavior and hiding of search result details text "
                   : u" Copy and Paste ";

    result->SetAccessibleName(title);
    result->SetTitle(title);
    result->SetDetails(u"Shortcuts");
    result->SetKeyboardShortcutTextVector(BuildKeyboardShortcutTextVector());
    results->Add(std::move(result));

    // Adding results will schedule Update().
    RunPendingMessages();
  }

  void SetupProgressBarAnswerCard() {
    SearchModel::SearchResults* results = GetResults();
    std::unique_ptr<TestSearchResult> result =
        std::make_unique<TestSearchResult>();
    result->set_display_type(ash::SearchResultDisplayType::kAnswerCard);
    result->set_best_match(true);

    result->SetAccessibleName(u"Memory 2.4GB | 7.6 GB total");
    result->SetDetails(u"Memory 2.4GB | 7.6 GB total");
    auto system_info_data =
        std::make_unique<ash::SystemInfoAnswerCardData>(0.5);

    result->SetSystemInfoAnswerCardData(*system_info_data.get());
    results->Add(std::move(result));

    // Adding results will schedule Update().
    RunPendingMessages();
  }

  int GetOpenResultCountAndReset(int ranking) {
    EXPECT_GT(view_delegate_.open_search_result_counts().count(ranking), 0u);
    int result = view_delegate_.open_search_result_counts()[ranking];
    view_delegate_.open_search_result_counts().clear();
    return result;
  }

  int GetUnifiedViewResultCount() const { return default_view_->num_results(); }

  void AddTestResult() {
    std::unique_ptr<TestSearchResult> result =
        std::make_unique<TestSearchResult>();
    result->set_display_type(ash::SearchResultDisplayType::kList);
    result->set_best_match(true);
    result->SetAccessibleName(u"Accessible Name");
    result->SetTitle(base::UTF8ToUTF16(
        base::StringPrintf("Added Result %d", GetUnifiedViewResultCount())));
    GetResults()->Add(std::move(result));
  }

  void DeleteResultAt(int index) { GetResults()->DeleteAt(index); }

  bool KeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::EventType::kKeyPressed, key_code, ui::EF_NONE);
    return default_view_->OnKeyPressed(event);
  }

  void ExpectConsistent() {
    RunPendingMessages();
    SearchModel::SearchResults* results = GetResults();

    for (size_t i = 0; i < results->item_count(); ++i) {
      SearchResultView* result_view = GetDefaultResultViewAt(i);
      ASSERT_TRUE(result_view) << "result view at " << i;
      EXPECT_EQ(results->GetItemAt(i), result_view->result());
    }
  }

  void DoUpdate() { default_view()->DoUpdate(); }

 private:
  // Needed by SearchResultInlineIconView.
  AshColorProvider ash_color_provider_;
  AppListTestViewDelegate view_delegate_;
  std::unique_ptr<SearchResultListView> default_view_;
  std::unique_ptr<SearchResultListView> answer_card_view_;
  raw_ptr<views::Widget, DanglingUntriaged> widget_;
};

TEST_F(SearchResultListViewTest, SpokenFeedback) {
  SetUpSearchResults();

  // Result 0 has a detail text. Expect that the detail is appended to the
  // accessibility name.
  EXPECT_EQ(u"Result 0, Detail",
            GetDefaultResultViewAt(0)->ComputeAccessibleName());

  // Result 2 has no detail text.
  EXPECT_EQ(u"Result 2", GetDefaultResultViewAt(2)->ComputeAccessibleName());
}

TEST_F(SearchResultListViewTest, KeyboardShortcutResult) {
  default_view()->SetBounds(0, 0, kPreferredWidth, 400);
  SetUpKeyboardShortcutResult();

  EXPECT_EQ(u"Copy and Paste",
            GetDefaultResultViewAt(0)->ComputeAccessibleName());
  EXPECT_TRUE(
      GetKeyboardShortcutContents(GetDefaultResultViewAt(0))->GetVisible());
}

// Verifies that title, details, and keyboard shortcut contents are shown for
// keyboard shortcut answer cards normally but details are hidden for results
// with long titles.
TEST_F(SearchResultListViewTest, KeyboardShortcutAnswerCard) {
  default_view()->SetBounds(0, 0, kPreferredWidth, 400);
  SetUpKeyboardShortcutAnswerCard(/*long_title=*/false);
  // Title, details,and keyboard shortcut views should be visible.
  EXPECT_TRUE(GetTitleContents(GetAnswerCardResultViewAt(0))->GetVisible());
  EXPECT_TRUE(GetDetailsContents(GetAnswerCardResultViewAt(0))->GetVisible());
  EXPECT_TRUE(
      GetResultTextSeparatorLabel(GetAnswerCardResultViewAt(0))->GetVisible());
  EXPECT_TRUE(
      GetKeyboardShortcutContents(GetAnswerCardResultViewAt(0))->GetVisible());

  // Delete the previous result.
  DeleteResultAt(0);

  SetUpKeyboardShortcutAnswerCard(/*long_title=*/true);
  // Title and keyboard shortcut views should be visible. The details view
  // is hidden because the long title view becomes multiline and takes priority.
  EXPECT_TRUE(
      GetKeyboardShortcutContents(GetAnswerCardResultViewAt(0))->GetVisible());
  EXPECT_TRUE(GetTitleContents(GetAnswerCardResultViewAt(0))->GetVisible());

  EXPECT_FALSE(
      GetResultTextSeparatorLabel(GetAnswerCardResultViewAt(0))->GetVisible());

  EXPECT_FALSE(GetDetailsContents(GetAnswerCardResultViewAt(0))->GetVisible());
}

// Verifies that details and progress contents are shown for system info answer
// cards which are of bar chart type normally
TEST_F(SearchResultListViewTest, ProgressBarAnswerCardTest) {
  default_view()->SetBounds(0, 0, kPreferredWidth, 400);
  SetupProgressBarAnswerCard();  // Details,and progress bar views should be
                                 // visible.
  EXPECT_FALSE(GetTitleContents(GetAnswerCardResultViewAt(0))->GetVisible());
  EXPECT_TRUE(GetDetailsContents(GetAnswerCardResultViewAt(0))->GetVisible());
  EXPECT_TRUE(
      GetProgressBarContents(GetAnswerCardResultViewAt(0))->GetVisible());

  EXPECT_FALSE(
      GetResultTextSeparatorLabel(GetAnswerCardResultViewAt(0))->GetVisible());
  EXPECT_FALSE(
      GetKeyboardShortcutContents(GetAnswerCardResultViewAt(0))->GetVisible());
}

TEST_F(SearchResultListViewTest, CorrectEnumLength) {
  EXPECT_EQ(
      // Check that all types except for SearchResultListType::kUnified are
      // included in GetAllListTypesForCategoricalSearch.
      static_cast<int>(SearchResultListView::SearchResultListType::kMaxValue) +
          1 /*0 indexing offset*/,
      static_cast<int>(
          SearchResultListView::GetAllListTypesForCategoricalSearch().size()));
  // Check that all types in AppListSearchResultCategory are included in
  // SearchResultListType.
  EXPECT_EQ(
      static_cast<int>(SearchResultListView::SearchResultListType::kMaxValue) +
          1 /*0 indexing offset*/ - num_list_types_not_in_category,
      static_cast<int>(SearchResult::Category::kMaxValue) +
          1 /*0 indexing offset*/ - num_category_without_list_type);
}

TEST_F(SearchResultListViewTest, SearchResultViewLayout) {
  // Set SearchResultListView bounds and check views are default size.
  default_view()->SetBounds(0, 0, kPreferredWidth, 400);
  SetUpSearchResults();
  // Override search result types.
  GetDefaultResultViewAt(0)->SetSearchResultViewType(
      SearchResultView::SearchResultViewType::kDefault);
  GetDefaultResultViewAt(1)->SetSearchResultViewType(
      SearchResultView::SearchResultViewType::kAnswerCard);
  DoUpdate();

  EXPECT_EQ(gfx::Size(kPreferredWidth, kDefaultViewHeight),
            GetDefaultResultViewAt(0)->size());
  EXPECT_EQ(GetDefaultResultViewAt(0)->TitleAndDetailsOrientationForTest(),
            views::LayoutOrientation::kHorizontal);
  EXPECT_EQ(gfx::Size(kPreferredWidth, kInlineAnswerViewHeight),
            GetDefaultResultViewAt(1)->size());
  EXPECT_EQ(GetDefaultResultViewAt(1)->TitleAndDetailsOrientationForTest(),
            views::LayoutOrientation::kVertical);
}

TEST_F(SearchResultListViewTest, BorderTest) {
  default_view()->SetBounds(0, 0, kPreferredWidth, 400);
  SetUpSearchResults();
  DoUpdate();
  EXPECT_EQ(kInlineAnswerBorder,
            GetAnswerCardResultViewAt(0)->GetBorder()->GetInsets());
  EXPECT_EQ(gfx::Insets(), GetDefaultResultViewAt(0)->GetBorder()->GetInsets());
}

TEST_F(SearchResultListViewTest, ModelObservers) {
  SetUpSearchResults();
  ExpectConsistent();

  // Remove from end.
  DeleteResultAt(kDefaultSearchItems - 1);
  ExpectConsistent();

  AddTestResult();
  ExpectConsistent();

  // Remove from end.
  DeleteResultAt(kDefaultSearchItems - 1);
  ExpectConsistent();

  AddTestResult();
  ExpectConsistent();

  // Delete from start.
  DeleteResultAt(0);
  ExpectConsistent();
}

}  // namespace test
}  // namespace ash
