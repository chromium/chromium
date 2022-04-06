// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

class SearchResultViewTest : public testing::Test {
 public:
  SearchResultViewTest() = default;
  SearchResultViewTest(const SearchResultViewTest&) = delete;
  SearchResultViewTest& operator=(const SearchResultViewTest&) = delete;
  ~SearchResultViewTest() override = default;
};

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
                     .expected_title_order = 2,
                     .expected_details_weight = 1,
                     .expected_details_order = 3},
                    {.total_width = 200,
                     .separator_width = 10,
                     .details_no_elide_width = 30,
                     .title_preferred_width = 100,
                     .details_preferred_width = 30,
                     .expected_title_weight = 1,
                     .expected_title_order = 2,
                     .expected_details_weight = 1,
                     .expected_details_order = 2},
                    {.total_width = 200,
                     .separator_width = 10,
                     .details_no_elide_width = 100,
                     .title_preferred_width = 200,
                     .details_preferred_width = 200,
                     .expected_title_weight = 110,
                     .expected_title_order = 3,
                     .expected_details_weight = 100,
                     .expected_details_order = 3}};
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
