// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_answer_card_view.h"

#include <memory>
#include <utility>

#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/test/test_search_result.h"
#include "ash/app_list/views/search_result_view.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/content/public/cpp/test/fake_navigable_contents.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/background.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace test {

namespace {
constexpr char kResultTitle[] = "The weather is fine";
constexpr double kDisplayScore = 13.0;
}  // namespace

class SearchResultAnswerCardViewTest : public views::ViewsTestBase {
 public:
  SearchResultAnswerCardViewTest() {}

  // Overridden from testing::Test:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    search_card_view_ = std::make_unique<views::View>();

    fake_card_contents_.set_default_response_headers(
        SearchResultAnswerCardView::CreateAnswerCardResponseHeadersForTest(
            "weather", kResultTitle));

    result_container_view_ = new SearchResultAnswerCardView(&view_delegate_);
    search_card_view_->AddChildView(result_container_view_);
    result_container_view_->SetResults(
        view_delegate_.GetSearchModel()->results());

    SetUpSearchResult();
  }

 protected:
  std::unique_ptr<TestSearchResult> CreateAnswerCardResult() {
    const GURL kFakeQueryUrl = GURL("https://www.google.com/coac?q=fake");
    auto result = std::make_unique<TestSearchResult>();
    result->set_display_type(ash::SearchResultDisplayType::kCard);
    result->set_title(base::UTF8ToUTF16(kResultTitle));
    result->set_display_score(kDisplayScore);
    result->set_query_url(kFakeQueryUrl);
    return result;
  }

  void SetUpSearchResult() {
    GetResults()->Add(CreateAnswerCardResult());

    // Adding results will schedule Update().
    view_delegate_.fake_navigable_contents_factory()
        .WaitForAndBindNextContentsRequest(&fake_card_contents_);
    RunPendingMessages();
  }

  int GetOpenResultCountAndReset(int ranking) {
    EXPECT_GT(view_delegate_.open_search_result_counts().count(ranking), 0u);
    int result = view_delegate_.open_search_result_counts()[ranking];
    view_delegate_.open_search_result_counts().clear();
    return result;
  }

  void DeleteResult() {
    GetResults()->DeleteAt(0);
    RunPendingMessages();
  }

  bool KeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    return result_container_view_->OnKeyPressed(event);
  }

  SearchModel::SearchResults* GetResults() {
    return view_delegate_.GetSearchModel()->results();
  }

  views::View* search_card_view() const { return search_card_view_.get(); }

  int GetYSize() const { return result_container_view_->GetYSize(); }

  int GetResultCountFromView() { return result_container_view_->num_results(); }

  double GetContainerScore() const {
    return result_container_view_->container_score();
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) {
    result_container_view_->children().front()->GetAccessibleNodeData(
        node_data);
  }

  AppListTestViewDelegate& view_delegate() { return view_delegate_; }

 private:
  AppListTestViewDelegate view_delegate_;

  // The root of the test's view hierarchy. In the real view hierarchy it's
  // SearchCardView.
  std::unique_ptr<views::View> search_card_view_;
  // Result container that we are testing. It's a child of search_card_view_.
  // Owned by the view hierarchy.
  SearchResultAnswerCardView* result_container_view_;

  // Fake NavigableContents implementation to back answer card navigations.
  content::FakeNavigableContents fake_card_contents_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultAnswerCardViewTest);
};

TEST_F(SearchResultAnswerCardViewTest, Basic) {
  EXPECT_EQ(kDisplayScore, GetContainerScore());
  EXPECT_EQ(1, GetResultCountFromView());
  ASSERT_TRUE(search_card_view()->GetVisible());
  EXPECT_EQ(1, GetYSize());
}

TEST_F(SearchResultAnswerCardViewTest, OpenResult) {
  EXPECT_TRUE(KeyPress(ui::VKEY_RETURN));
  EXPECT_EQ(1, GetOpenResultCountAndReset(0));
}

TEST_F(SearchResultAnswerCardViewTest, SpokenFeedback) {
  ui::AXNodeData node_data;
  GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, node_data.role);
  EXPECT_EQ(kResultTitle,
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST_F(SearchResultAnswerCardViewTest, DeleteResult) {
  DeleteResult();
  EXPECT_EQ(0UL, GetResults()->item_count());
  EXPECT_EQ(0, GetYSize());
  ASSERT_FALSE(search_card_view()->GetVisible());
  EXPECT_EQ(0, GetContainerScore());
}

TEST_F(SearchResultAnswerCardViewTest, RemoveEquivalent) {
  // Ensures no results.
  DeleteResult();

  // Creates a result that will be removed later when answer card loads.
  constexpr char kEquivalentResultId[] = "equivalent-id";
  auto result = std::make_unique<TestSearchResult>();
  result->set_result_id(kEquivalentResultId);
  result->set_display_type(ash::SearchResultDisplayType::kList);
  result->set_display_score(kDisplayScore);
  GetResults()->Add(std::move(result));

  // Creates an answer card result and associated with an equivalent result id.
  result = CreateAnswerCardResult();
  result->set_equivalent_result_id(kEquivalentResultId);
  GetResults()->Add(std::move(result));

  EXPECT_EQ(2u, GetResults()->item_count());
  EXPECT_TRUE(
      view_delegate().GetSearchModel()->FindSearchResult(kEquivalentResultId));

  // Wait for the answer card result to load.
  RunPendingMessages();

  // Equivalent result should be removed.
  EXPECT_EQ(1u, GetResults()->item_count());
  EXPECT_FALSE(
      view_delegate().GetSearchModel()->FindSearchResult(kEquivalentResultId));
}

}  // namespace test
}  // namespace ash
