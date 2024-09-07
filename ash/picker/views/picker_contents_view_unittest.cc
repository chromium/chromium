// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_contents_view.h"

#include "ash/picker/views/picker_style.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;

using PickerContentsViewTest = views::ViewsTestBase;

TEST_F(PickerContentsViewTest, DefaultHasNoChildren) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  auto* view = widget->SetContentsView(std::make_unique<PickerContentsView>(
      PickerLayoutType::kMainResultsBelowSearchField));

  EXPECT_THAT(view->page_container_for_testing()->children(), IsEmpty());
}

TEST_F(PickerContentsViewTest, AddPageFirstChildIsVisible) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  auto* view = widget->SetContentsView(std::make_unique<PickerContentsView>(
      PickerLayoutType::kMainResultsBelowSearchField));
  auto* page1 = view->AddPage(std::make_unique<views::View>());
  auto* page2 = view->AddPage(std::make_unique<views::View>());

  EXPECT_THAT(
      view->page_container_for_testing()->children(),
      ElementsAre(
          AllOf(page1, Pointee(Property(&views::View::GetVisible, true))),
          AllOf(page2, Pointee(Property(&views::View::GetVisible, false)))));
}

TEST_F(PickerContentsViewTest, SetActivePageChangesVisibility) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* view = widget->SetContentsView(std::make_unique<PickerContentsView>(
      PickerLayoutType::kMainResultsBelowSearchField));
  auto* page1 = view->AddPage(std::make_unique<views::View>());
  auto* page2 = view->AddPage(std::make_unique<views::View>());

  view->SetActivePage(page1);

  EXPECT_THAT(
      view->page_container_for_testing()->children(),
      ElementsAre(
          AllOf(page1, Pointee(Property(&views::View::GetVisible, true))),
          AllOf(page2, Pointee(Property(&views::View::GetVisible, false)))));

  view->SetActivePage(page2);

  EXPECT_THAT(
      view->page_container_for_testing()->children(),
      ElementsAre(
          AllOf(page1, Pointee(Property(&views::View::GetVisible, false))),
          AllOf(page2, Pointee(Property(&views::View::GetVisible, true)))));
}

}  // namespace
}  // namespace ash
