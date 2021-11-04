// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace test {

namespace {
int kDefaultSearchItems = 5;

// Preferred sizing for different types of search result views.
constexpr int kPreferredWidth = 640;
constexpr int kClassicViewHeight = 48;
constexpr int kDefaultViewHeight = 40;
constexpr int kInlineAnswerViewHeight = 80;

// SearchResultListType::kUnified and SearchResultListType::kBestMatch do not
// have associated categories.
constexpr int num_list_types_not_in_category = 2;
// SearchResult::Category::kUnknown does not have an associated list type.
constexpr int num_category_without_list_type = 1;
// SearchResultListType::kUnified is used for categorical search.
constexpr int num_list_types_not_used_for_categorical_search = 1;

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
    view_ = std::make_unique<SearchResultListView>(nullptr, &view_delegate_);
    view_->SetListType(SearchResultListView::SearchResultListType::kUnified);
    widget_->SetBounds(gfx::Rect(0, 0, 700, 500));
    widget_->GetContentsView()->AddChildView(view_.get());
    widget_->Show();
    view_->SetResults(GetResults());
  }

  void TearDown() override {
    view_.reset();
    widget_->CloseNow();
    views::test::WidgetTest::TearDown();
  }

 protected:
  SearchResultListView* view() const { return view_.get(); }

  SearchResultView* GetResultViewAt(int index) const {
    return view_->GetResultViewAt(index);
  }

  std::vector<SearchResultView*> GetAssistantResultViews() const {
    std::vector<SearchResultView*> results;
    for (auto* view : view_->search_result_views_) {
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
    assistant_result->set_title(u"assistant result");
    results->Add(std::move(assistant_result));

    RunPendingMessages();
  }

  void SetUpSearchResults() {
    SearchModel::SearchResults* results = GetResults();
    for (int i = 0; i < kDefaultSearchItems; ++i) {
      std::unique_ptr<TestSearchResult> result =
          std::make_unique<TestSearchResult>();
      result->set_display_type(ash::SearchResultDisplayType::kList);
      result->set_title(base::UTF8ToUTF16(base::StringPrintf("Result %d", i)));
      if (i < 2)
        result->set_details(u"Detail");
      results->Add(std::move(result));
    }

    // Adding results will schedule Update().
    RunPendingMessages();
  }

  int GetOpenResultCountAndReset(int ranking) {
    EXPECT_GT(view_delegate_.open_search_result_counts().count(ranking), 0u);
    int result = view_delegate_.open_search_result_counts()[ranking];
    view_delegate_.open_search_result_counts().clear();
    return result;
  }

  int GetResultCount() const { return view_->num_results(); }

  void AddTestResultAtIndex(int index) {
    GetResults()->Add(std::make_unique<TestSearchResult>());
  }

  void DeleteResultAt(int index) { GetResults()->DeleteAt(index); }

  bool KeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    return view_->OnKeyPressed(event);
  }

  void ExpectConsistent() {
    // Adding results will schedule Update().
    RunPendingMessages();

    SearchModel::SearchResults* results = GetResults();
    for (size_t i = 0; i < results->item_count(); ++i) {
      EXPECT_EQ(results->GetItemAt(i), GetResultViewAt(i)->result());
    }
  }

  void DoUpdate() { view()->DoUpdate(); }

 private:
  TestAppListColorProvider color_provider_;  // Needed by AppListView.
  AppListTestViewDelegate view_delegate_;
  std::unique_ptr<SearchResultListView> view_;
  views::Widget* widget_;
};

TEST_F(SearchResultListViewTest, SpokenFeedback) {
  SetUpSearchResults();

  // Result 0 has a detail text. Expect that the detail is appended to the
  // accessibility name.
  EXPECT_EQ(u"Result 0, Detail", GetResultViewAt(0)->ComputeAccessibleName());

  // Result 2 has no detail text.
  EXPECT_EQ(u"Result 2", GetResultViewAt(2)->ComputeAccessibleName());
}

TEST_F(SearchResultListViewTest, CorrectEnumLength) {
  EXPECT_EQ(
      // Check that all types except for SearchResultListType::kUnified are
      // included in GetAllListTypesForCategoricalSearch.
      static_cast<int>(SearchResultListView::SearchResultListType::kMaxValue) +
          1 /*0 indexing offset*/,
      static_cast<int>(
          SearchResultListView::GetAllListTypesForCategoricalSearch().size() +
          num_list_types_not_used_for_categorical_search));
  // Check that all types in AppListSearchResultCategory are included in
  // SearchResultListType.
  DCHECK(
      static_cast<int>(SearchResultListView::SearchResultListType::kMaxValue) +
          1 /*0 indexing offset*/ - num_list_types_not_in_category ==
      static_cast<int>(SearchResult::Category::kMaxValue) +
          1 /*0 indexing offset*/ - num_category_without_list_type);
}

TEST_F(SearchResultListViewTest, SearchResultViewPreferredSize) {
  // Set SearchResultListView bounds and check views are default size.
  view()->SetBounds(0, 0, kPreferredWidth, 400);
  SetUpSearchResults();
  EXPECT_EQ(gfx::Size(kPreferredWidth, kClassicViewHeight),
            GetResultViewAt(0)->size());
  EXPECT_EQ(gfx::Size(kPreferredWidth, kClassicViewHeight),
            GetResultViewAt(1)->size());
  EXPECT_EQ(gfx::Size(kPreferredWidth, kClassicViewHeight),
            GetResultViewAt(2)->size());

  // Override search result tpyes
  GetResultViewAt(0)->SetSearchResultViewType(
      SearchResultView::SearchResultViewType::kClassic);
  GetResultViewAt(1)->SetSearchResultViewType(
      SearchResultView::SearchResultViewType::kInlineAnswer);
  GetResultViewAt(2)->SetSearchResultViewType(
      SearchResultView::SearchResultViewType::kDefault);
  DoUpdate();

  EXPECT_EQ(gfx::Size(kPreferredWidth, kClassicViewHeight),
            GetResultViewAt(0)->size());
  EXPECT_EQ(gfx::Size(kPreferredWidth, kInlineAnswerViewHeight),
            GetResultViewAt(1)->size());
  EXPECT_EQ(gfx::Size(kPreferredWidth, kDefaultViewHeight),
            GetResultViewAt(2)->size());
}

TEST_F(SearchResultListViewTest, ModelObservers) {
  SetUpSearchResults();
  ExpectConsistent();

  // Remove from end.
  DeleteResultAt(kDefaultSearchItems - 1);
  ExpectConsistent();

  // Insert at start.
  AddTestResultAtIndex(0);
  ExpectConsistent();

  // Remove from end.
  DeleteResultAt(kDefaultSearchItems - 1);
  ExpectConsistent();

  // Insert at end.
  AddTestResultAtIndex(kDefaultSearchItems);
  ExpectConsistent();

  // Delete from start.
  DeleteResultAt(0);
  ExpectConsistent();
}

TEST_F(SearchResultListViewTest, HidesAssistantResultWhenTilesVisible) {
  SetUpSearchResults();

  // No assistant results available.
  EXPECT_TRUE(GetAssistantResultViews().empty());

  AddAssistantSearchResult();

  // Assistant result should be set and visible.
  for (const auto* view : GetAssistantResultViews()) {
    EXPECT_TRUE(view->GetVisible());
    EXPECT_EQ(view->result()->title(), u"assistant result");
  }

  // Add a tile result
  std::unique_ptr<TestSearchResult> tile_result =
      std::make_unique<TestSearchResult>();
  tile_result->set_display_type(ash::SearchResultDisplayType::kTile);
  GetResults()->Add(std::move(tile_result));

  RunPendingMessages();

  // Assistant result should be gone.
  EXPECT_TRUE(GetAssistantResultViews().empty());
}

}  // namespace test
}  // namespace ash
