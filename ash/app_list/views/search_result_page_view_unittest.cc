// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_page_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/test/test_search_result.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_tile_item_list_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/test/views_test_base.h"

namespace {

enum class AnswerCardState {
  ANSWER_CARD_OFF,
  ANSWER_CARD_ON_WITH_RESULT,
  ANSWER_CARD_ON_WITHOUT_RESULT,
};

}  // namespace

namespace app_list {
namespace test {

class SearchResultPageViewTest
    : public views::ViewsTestBase,
      public testing::WithParamInterface<AnswerCardState> {
 public:
  SearchResultPageViewTest() = default;
  ~SearchResultPageViewTest() override = default;

  // Overridden from testing::Test:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    // Reading test parameters.
    bool test_with_answer_card = true;
    if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
      const AnswerCardState answer_card_state = GetParam();
      test_with_answer_card =
          answer_card_state != AnswerCardState::ANSWER_CARD_OFF;
    }

    // Setting up the feature set.
    if (test_with_answer_card)
      scoped_feature_list_.InitAndEnableFeature(
          app_list_features::kEnableAnswerCard);
    else
      scoped_feature_list_.InitAndDisableFeature(
          app_list_features::kEnableAnswerCard);

    ASSERT_EQ(test_with_answer_card, app_list_features::IsAnswerCardEnabled());

    // Setting up views.
    delegate_ = std::make_unique<AppListTestViewDelegate>();
    app_list_view_ = new AppListView(delegate_.get());
    AppListView::InitParams params;
    params.parent = GetContext();
    app_list_view_->Initialize(params);
    app_list_view_->GetWidget()->Show();

    ContentsView* contents_view =
        app_list_view_->app_list_main_view()->contents_view();
    view_ = contents_view->search_results_page_view();
    tile_list_view_ =
        contents_view->search_result_tile_item_list_view_for_test();
    list_view_ = contents_view->search_result_list_view_for_test();
  }
  void TearDown() override {
    app_list_view_->GetWidget()->Close();
    views::ViewsTestBase::TearDown();
  }

 protected:
  SearchResultPageView* view() const { return view_; }

  SearchResultTileItemListView* tile_list_view() const {
    return tile_list_view_;
  }
  SearchResultListView* list_view() const { return list_view_; }

  SearchModel::SearchResults* GetResults() const {
    return delegate_->GetSearchModel()->results();
  }

 private:
  AppListView* app_list_view_ = nullptr;  // Owned by native widget.
  SearchResultPageView* view_ = nullptr;  // Owned by views hierarchy.
  SearchResultTileItemListView* tile_list_view_ =
      nullptr;                                 // Owned by views hierarchy.
  SearchResultListView* list_view_ = nullptr;  // Owned by views hierarchy.
  std::unique_ptr<AppListTestViewDelegate> delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultPageViewTest);
};

// Instantiate the Boolean which is used to toggle answer cards in
// the parameterized tests.
INSTANTIATE_TEST_CASE_P(
    ,
    SearchResultPageViewTest,
    ::testing::Values(AnswerCardState::ANSWER_CARD_OFF,
                      AnswerCardState::ANSWER_CARD_ON_WITHOUT_RESULT,
                      AnswerCardState::ANSWER_CARD_ON_WITH_RESULT));

TEST_P(SearchResultPageViewTest, ResultsSorted) {
  SearchModel::SearchResults* results = GetResults();

  // Add 3 results and expect the tile list view to be the first result
  // container view.
  TestSearchResult* tile_result = new TestSearchResult();
  tile_result->set_display_type(ash::SearchResultDisplayType::kTile);
  tile_result->set_display_score(1.0);
  results->Add(base::WrapUnique(tile_result));
  {
    TestSearchResult* list_result = new TestSearchResult();
    list_result->set_display_type(ash::SearchResultDisplayType::kList);
    list_result->set_display_score(0.5);
    results->Add(base::WrapUnique(list_result));
  }
  {
    TestSearchResult* list_result = new TestSearchResult();
    list_result->set_display_type(ash::SearchResultDisplayType::kList);
    list_result->set_display_score(0.3);
    results->Add(base::WrapUnique(list_result));
  }

  // Adding results will schedule Update().
  RunPendingMessages();

  EXPECT_EQ(tile_list_view(), view()->result_container_views()[0]);
  EXPECT_EQ(list_view(), view()->result_container_views()[1]);

  // Change the relevance of the tile result and expect the list results to be
  // displayed first.
  // TODO(warx): fullscreen launcher should always have tile list view to be
  // displayed first over list view.
  tile_result->set_display_score(0.4);

  results->NotifyItemsChanged(0, 1);
  RunPendingMessages();

  EXPECT_EQ(list_view(), view()->result_container_views()[0]);
  EXPECT_EQ(tile_list_view(), view()->result_container_views()[1]);
}

}  // namespace test
}  // namespace app_list
