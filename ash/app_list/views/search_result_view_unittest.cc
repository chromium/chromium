// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_view.h"
#include <memory>

#include "ash/app_list/model/search/test_search_result.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/test/widget_test.h"

namespace ash {

class SearchResultViewTest : public testing::Test {
 public:
  SearchResultViewTest() = default;
  SearchResultViewTest(const SearchResultViewTest&) = delete;
  SearchResultViewTest& operator=(const SearchResultViewTest&) = delete;
  ~SearchResultViewTest() override = default;
};

class SearchResultViewWidgetTest : public views::test::WidgetTest {
 public:
  SearchResultViewWidgetTest() = default;

  SearchResultViewWidgetTest(const SearchResultViewWidgetTest&) = delete;
  SearchResultViewWidgetTest& operator=(const SearchResultViewWidgetTest&) =
      delete;

  ~SearchResultViewWidgetTest() override = default;

  void SetUp() override {
    views::test::WidgetTest::SetUp();

    widget_ = CreateTopLevelPlatformWidget();

    answer_card_view_ = std::make_unique<SearchResultView>(
        /*list_view=*/nullptr, /*view_delegate=*/nullptr,
        /*dialog_controller=*/nullptr,
        SearchResultView::SearchResultViewType::kAnswerCard);
    search_result_view_ = std::make_unique<SearchResultView>(
        /*list_view=*/nullptr, /*view_delegate=*/nullptr,
        /*dialog_controller=*/nullptr,
        SearchResultView::SearchResultViewType::kDefault);

    widget_->SetBounds(gfx::Rect(0, 0, 740, 300));

    widget_->GetContentsView()->AddChildView(answer_card_view_.get());
  }

  void TearDown() override {
    answer_card_view_.reset();
    widget_->CloseNow();
    views::test::WidgetTest::TearDown();
  }

  SearchResultView* answer_card_view() { return answer_card_view_.get(); }
  SearchResultView* search_result_view() { return search_result_view_.get(); }

  views::LayoutOrientation GetTitleDetailsContainerOrientation(
      SearchResultView* view) {
    return view->title_and_details_container_->GetOrientation();
  }

  std::u16string GetTitleText(SearchResultView* view) {
    std::u16string merged_string = u"";
    for (const auto& label_tag_pair : view->title_label_tags_)
      merged_string += label_tag_pair.GetLabel()->GetText();
    return merged_string;
  }

  std::u16string GetDetailsText(SearchResultView* view) {
    std::u16string merged_string = u"";
    for (const auto& label_tag_pair : view->details_label_tags_)
      merged_string += label_tag_pair.GetLabel()->GetText();
    return merged_string;
  }

  std::u16string GetRightDetailsText(SearchResultView* view) {
    std::u16string merged_string = u"";
    for (const auto& label_tag_pair : view->right_details_label_tags_) {
      merged_string += label_tag_pair.GetLabel()->GetText();
    }
    return merged_string;
  }

  bool IsProgressBarChart(SearchResultView* view) {
    return view->is_progress_bar_answer_card_;
  }

  void SetSearchResultViewMultilineDetailsHeight(
      SearchResultView* search_result_view,
      int height) {
    search_result_view->set_multi_line_details_height_for_test(height);
  }

  void SetSearchResultViewMultilineTitleHeight(
      SearchResultView* search_result_view,
      int height) {
    search_result_view->set_multi_line_title_height_for_test(height);
  }

  int SearchResultViewPreferredHeight(SearchResultView* search_result_view) {
    return search_result_view->PreferredHeight();
  }

  void SetupTestSearchResult(TestSearchResult* result) {
    std::vector<SearchResult::TextItem> title_text_vector;
    SearchResult::TextItem title_text_item_1(
        ash::SearchResultTextItemType::kString);
    title_text_item_1.SetText(u"Test Search");
    title_text_item_1.SetTextTags({});
    title_text_vector.push_back(title_text_item_1);
    SearchResult::TextItem title_text_item_2(
        ash::SearchResultTextItemType::kString);
    title_text_item_2.SetText(u" Result Title " +
                              base::NumberToString16(result_id));
    title_text_item_2.SetTextTags({});
    title_text_vector.push_back(title_text_item_2);
    result->SetTitleTextVector(title_text_vector);

    std::vector<SearchResult::TextItem> details_text_vector;
    SearchResult::TextItem details_text_item_1(
        ash::SearchResultTextItemType::kString);
    details_text_item_1.SetText(u"Test");
    details_text_item_1.SetTextTags({});
    details_text_vector.push_back(details_text_item_1);
    SearchResult::TextItem details_text_item_2(
        ash::SearchResultTextItemType::kString);
    details_text_item_2.SetText(u" Search Result Details " +
                                base::NumberToString16(result_id));
    details_text_item_2.SetTextTags({});
    details_text_vector.push_back(details_text_item_2);

    result->set_result_id("Test Search Result " +
                          base::NumberToString(result_id));
    result->SetDetailsTextVector(details_text_vector);

    result_id++;
  }

 private:
  int result_id = 0;
  std::unique_ptr<SearchResultView> answer_card_view_;
  std::unique_ptr<SearchResultView> search_result_view_;
  raw_ptr<views::Widget, DanglingUntriaged> widget_;
};

TEST_F(SearchResultViewWidgetTest, SearchResultTextVectorUpdate) {
  auto answer_card_result_0 = std::make_unique<TestSearchResult>();
  SetupTestSearchResult(answer_card_result_0.get());
  answer_card_view()->SetResult(answer_card_result_0.get());
  answer_card_view()->OnResultChanged();
  EXPECT_EQ(u"Test Search Result Title 0", GetTitleText(answer_card_view()));
  EXPECT_EQ(u"Test Search Result Details 0",
            GetDetailsText(answer_card_view()));

  auto answer_card_result_1 = std::make_unique<TestSearchResult>();
  SetupTestSearchResult(answer_card_result_1.get());
  answer_card_view()->SetResult(answer_card_result_1.get());
  answer_card_view()->OnResultChanged();
  EXPECT_EQ(u"Test Search Result Title 1", GetTitleText(answer_card_view()));
  EXPECT_EQ(u"Test Search Result Details 1",
            GetDetailsText(answer_card_view()));

  auto default_result_0 = std::make_unique<TestSearchResult>();
  SetupTestSearchResult(default_result_0.get());
  search_result_view()->SetResult(default_result_0.get());
  search_result_view()->OnResultChanged();
  EXPECT_EQ(u"Test Search Result Title 2", GetTitleText(search_result_view()));
  EXPECT_EQ(u"Test Search Result Details 2",
            GetDetailsText(search_result_view()));

  auto default_result_1 = std::make_unique<TestSearchResult>();
  SetupTestSearchResult(default_result_1.get());
  search_result_view()->SetResult(default_result_1.get());
  search_result_view()->OnResultChanged();
  EXPECT_EQ(u"Test Search Result Title 3", GetTitleText(search_result_view()));
  EXPECT_EQ(u"Test Search Result Details 3",
            GetDetailsText(search_result_view()));
}

TEST_F(SearchResultViewWidgetTest, TitleAndDetailsContainerOrientationTest) {
  EXPECT_EQ(GetTitleDetailsContainerOrientation(search_result_view()),
            views::LayoutOrientation::kHorizontal);
  EXPECT_EQ(GetTitleDetailsContainerOrientation(answer_card_view()),
            views::LayoutOrientation::kVertical);
}

TEST_F(SearchResultViewWidgetTest, PreferredHeight) {
  static constexpr struct TestCase {
    int multi_line_details_height;
    int multi_line_title_height;
    int preferred_height;
  } kTestCases[] = {{.multi_line_details_height = 0,
                     .multi_line_title_height = 0,
                     .preferred_height = 88},
                    {.multi_line_details_height = 0,
                     .multi_line_title_height = 20,
                     .preferred_height = 88},
                    {.multi_line_details_height = 0,
                     .multi_line_title_height = 40,
                     .preferred_height = 108},
                    {.multi_line_details_height = 18,
                     .multi_line_title_height = 0,
                     .preferred_height = 88},
                    {.multi_line_details_height = 18,
                     .multi_line_title_height = 20,
                     .preferred_height = 88},
                    {.multi_line_details_height = 18,
                     .multi_line_title_height = 40,
                     .preferred_height = 108},
                    {.multi_line_details_height = 36,
                     .multi_line_title_height = 0,
                     .preferred_height = 106},
                    {.multi_line_details_height = 36,
                     .multi_line_title_height = 20,
                     .preferred_height = 106},
                    {.multi_line_details_height = 36,
                     .multi_line_title_height = 40,
                     .preferred_height = 126},
                    {.multi_line_details_height = 54,
                     .multi_line_title_height = 0,
                     .preferred_height = 124},
                    {.multi_line_details_height = 54,
                     .multi_line_title_height = 20,
                     .preferred_height = 124},
                    {.multi_line_details_height = 54,
                     .multi_line_title_height = 40,
                     .preferred_height = 144}};
  for (auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Test case: {multi_line_details_height: "
                 << test_case.multi_line_details_height
                 << " multi_line_title_height: "
                 << test_case.multi_line_title_height << "}");
    SetSearchResultViewMultilineDetailsHeight(
        answer_card_view(), test_case.multi_line_details_height);
    SetSearchResultViewMultilineTitleHeight(answer_card_view(),
                                            test_case.multi_line_title_height);
    EXPECT_EQ(test_case.preferred_height,
              SearchResultViewPreferredHeight(answer_card_view()));
  }
}

TEST_F(SearchResultViewTest, FlexWeightCalculation) {
  static constexpr struct TestCase {
    int total_width;
    int separator_width;
    int details_no_elide_width;
    int title_preferred_width;
    int details_preferred_width;
    int expected_title_weight;
    int expected_title_order;
    int expected_details_weight;
    int expected_details_order;
  } kTestCases[] = {{.total_width = 200,
                     .separator_width = 10,
                     .details_no_elide_width = 30,
                     .title_preferred_width = 100,
                     .details_preferred_width = 100,
                     .expected_title_weight = 1,
                     .expected_title_order = 1,
                     .expected_details_weight = 1,
                     .expected_details_order = 2},
                    {.total_width = 200,
                     .separator_width = 10,
                     .details_no_elide_width = 30,
                     .title_preferred_width = 100,
                     .details_preferred_width = 30,
                     .expected_title_weight = 1,
                     .expected_title_order = 1,
                     .expected_details_weight = 1,
                     .expected_details_order = 1},
                    {.total_width = 200,
                     .separator_width = 10,
                     .details_no_elide_width = 100,
                     .title_preferred_width = 200,
                     .details_preferred_width = 200,
                     .expected_title_weight = 110,
                     .expected_title_order = 2,
                     .expected_details_weight = 100,
                     .expected_details_order = 2}};
  for (auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Test case: {total_width: " << test_case.total_width
                 << ", separator_width: " << test_case.separator_width
                 << ", details_no_elide_width: "
                 << test_case.details_no_elide_width
                 << ", title_preferred_width: "
                 << test_case.title_preferred_width
                 << ", details_preferred_width: "
                 << test_case.details_preferred_width << "}");

    auto title_flex_layout_view = std::make_unique<views::FlexLayoutView>();
    title_flex_layout_view->SetPreferredSize(
        gfx::Size(test_case.title_preferred_width, 1));
    auto details_flex_layout_view = std::make_unique<views::FlexLayoutView>();
    details_flex_layout_view->SetPreferredSize(
        gfx::Size(test_case.details_preferred_width, 1));

    SearchResultView::SetFlexBehaviorForTextContents(
        test_case.total_width, test_case.separator_width,
        test_case.details_no_elide_width, title_flex_layout_view.get(),
        details_flex_layout_view.get());
    EXPECT_EQ(test_case.expected_title_weight,
              title_flex_layout_view.get()
                  ->GetProperty(views::kFlexBehaviorKey)
                  ->weight());
    EXPECT_EQ(test_case.expected_title_order,
              title_flex_layout_view.get()
                  ->GetProperty(views::kFlexBehaviorKey)
                  ->order());
    EXPECT_EQ(test_case.expected_details_weight,
              details_flex_layout_view.get()
                  ->GetProperty(views::kFlexBehaviorKey)
                  ->weight());
    EXPECT_EQ(test_case.expected_details_order,
              details_flex_layout_view.get()
                  ->GetProperty(views::kFlexBehaviorKey)
                  ->order());
  }
}

TEST_F(SearchResultViewWidgetTest, ProgressBarResult) {
  auto progress_bar_result = std::make_unique<TestSearchResult>();
  auto system_info_data = std::make_unique<ash::SystemInfoAnswerCardData>(0.5);
  system_info_data->SetExtraDetails(u"right description");
  progress_bar_result->SetSystemInfoAnswerCardData(*system_info_data.get());
  SetupTestSearchResult(progress_bar_result.get());
  answer_card_view()->SetResult(progress_bar_result.get());
  answer_card_view()->OnResultChanged();
  EXPECT_EQ(true, IsProgressBarChart(answer_card_view()));
  EXPECT_EQ(u"Test Search Result Details 0",
            GetDetailsText(answer_card_view()));
  EXPECT_EQ(u"right description", GetRightDetailsText(answer_card_view()));
}

}  // namespace ash
