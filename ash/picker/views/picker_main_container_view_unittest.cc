// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_main_container_view.h"

#include "ash/picker/metrics/picker_performance_metrics.h"
#include "ash/picker/views/picker_contents_view.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "ash/picker/views/picker_page_view.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_style.h"
#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/views/background.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class MockPickerPageView : public PickerPageView {
  METADATA_HEADER(MockPickerPageView, PickerPageView)

 public:
  MockPickerPageView() = default;
  MockPickerPageView(const MockPickerPageView&) = delete;
  MockPickerPageView& operator=(const MockPickerPageView&) = delete;
  ~MockPickerPageView() override = default;

  // PickerPageView:
  views::View* GetTopItem() override { return nullptr; }
  views::View* GetBottomItem() override { return nullptr; }
  views::View* GetItemAbove(views::View* item) override { return nullptr; }
  views::View* GetItemBelow(views::View* item) override { return nullptr; }
  views::View* GetItemLeftOf(views::View* item) override { return nullptr; }
  views::View* GetItemRightOf(views::View* item) override { return nullptr; }
  bool ContainsItem(views::View* item) override { return true; }
};

BEGIN_METADATA(MockPickerPageView)
END_METADATA

using PickerMainContainerViewTest = views::ViewsTestBase;

TEST_F(PickerMainContainerViewTest, BackgroundColor) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* container =
      widget->SetContentsView(std::make_unique<PickerMainContainerView>());

  EXPECT_EQ(container->background()->get_color(),
            container->GetColorProvider()->GetColor(
                cros_tokens::kCrosSysSystemBaseElevatedOpaque));
}

TEST_F(PickerMainContainerViewTest, LayoutWithContentsBelowSearchField) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* container =
      widget->SetContentsView(std::make_unique<PickerMainContainerView>());

  auto* search_field =
      container->AddSearchFieldView(std::make_unique<PickerSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  PickerContentsView* contents = container->AddContentsView(
      PickerLayoutType::kMainResultsBelowSearchField);

  EXPECT_GE(contents->GetBoundsInScreen().y(),
            search_field->GetBoundsInScreen().bottom());
}

TEST_F(PickerMainContainerViewTest, LayoutWithContentsAboveSearchField) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* container =
      widget->SetContentsView(std::make_unique<PickerMainContainerView>());

  auto* search_field =
      container->AddSearchFieldView(std::make_unique<PickerSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  PickerContentsView* contents = container->AddContentsView(
      PickerLayoutType::kMainResultsAboveSearchField);

  EXPECT_LE(contents->GetBoundsInScreen().bottom(),
            search_field->GetBoundsInScreen().y());
}

TEST_F(PickerMainContainerViewTest, SetsActivePage) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* container =
      widget->SetContentsView(std::make_unique<PickerMainContainerView>());
  container->AddContentsView(PickerLayoutType::kMainResultsBelowSearchField);
  auto* page1 = container->AddPage(std::make_unique<MockPickerPageView>());
  auto* page2 = container->AddPage(std::make_unique<MockPickerPageView>());

  container->SetActivePage(page1);

  EXPECT_EQ(container->active_page(), page1);
  EXPECT_TRUE(page1->GetVisible());
  EXPECT_FALSE(page2->GetVisible());

  container->SetActivePage(page2);

  EXPECT_EQ(container->active_page(), page2);
  EXPECT_FALSE(page1->GetVisible());
  EXPECT_TRUE(page2->GetVisible());
}

}  // namespace
}  // namespace ash
