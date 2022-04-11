// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_view.h"

#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
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

    widget_->SetBounds(gfx::Rect(0, 0, 740, 200));

    widget_->GetContentsView()->AddChildView(answer_card_view_.get());
  }

  void TearDown() override {
    answer_card_view_.reset();
    widget_->CloseNow();
    views::test::WidgetTest::TearDown();
  }

  SearchResultView* answer_card_view() { return answer_card_view_.get(); }

  void SetSearchResultViewMultilineLabelHeight(
      SearchResultView* search_result_view,
      int height) {
    search_result_view->set_multi_line_label_height_for_test(height);
  }

  int SearchResultViewPreferredHeight(SearchResultView* search_result_view) {
    return search_result_view->PreferredHeight();
  }

 private:
  TestAppListColorProvider color_provider_;  // Needed by AppListView.
  std::unique_ptr<SearchResultView> answer_card_view_;
  views::Widget* widget_;
};

TEST_F(SearchResultViewWidgetTest, PreferredHeight) {
  static constexpr struct TestCase {
    int multi_line_label_height;
    int preferred_height;
  } kTestCases[] = {{.multi_line_label_height = 0, .preferred_height = 80},
                    {.multi_line_label_height = 18, .preferred_height = 80},
                    {.multi_line_label_height = 36, .preferred_height = 98},
                    {.multi_line_label_height = 54, .preferred_height = 116}};

  for (auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Test case: {multi_line_label_height: "
                 << test_case.multi_line_label_height << "}");
    SetSearchResultViewMultilineLabelHeight(answer_card_view(),
                                            test_case.multi_line_label_height);
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

}  // namespace ash
