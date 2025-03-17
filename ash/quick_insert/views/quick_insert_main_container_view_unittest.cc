// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_main_container_view.h"

#include "ash/quick_insert/metrics/quick_insert_performance_metrics.h"
#include "ash/quick_insert/views/quick_insert_contents_view.h"
#include "ash/quick_insert/views/quick_insert_key_event_handler.h"
#include "ash/quick_insert/views/quick_insert_page_view.h"
#include "ash/quick_insert/views/quick_insert_search_field_view.h"
#include "ash/quick_insert/views/quick_insert_style.h"
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

class MockQuickInsertPageView : public QuickInsertPageView {
  METADATA_HEADER(MockQuickInsertPageView, QuickInsertPageView)

 public:
  MockQuickInsertPageView() = default;
  MockQuickInsertPageView(const MockQuickInsertPageView&) = delete;
  MockQuickInsertPageView& operator=(const MockQuickInsertPageView&) = delete;
  ~MockQuickInsertPageView() override = default;

  // QuickInsertPageView:
  views::View* GetTopItem() override { return nullptr; }
  views::View* GetBottomItem() override { return nullptr; }
  views::View* GetItemAbove(views::View* item) override { return nullptr; }
  views::View* GetItemBelow(views::View* item) override { return nullptr; }
  views::View* GetItemLeftOf(views::View* item) override { return nullptr; }
  views::View* GetItemRightOf(views::View* item) override { return nullptr; }
  bool ContainsItem(views::View* item) override { return true; }
};

BEGIN_METADATA(MockQuickInsertPageView)
END_METADATA

using QuickInsertMainContainerViewTest = views::ViewsTestBase;

TEST_F(QuickInsertMainContainerViewTest, BackgroundColor) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* container =
      widget->SetContentsView(std::make_unique<QuickInsertMainContainerView>());

  EXPECT_EQ(container->background()->color(),
            cros_tokens::kCrosSysSystemBaseElevatedOpaque);
}

TEST_F(QuickInsertMainContainerViewTest, LayoutWithContentsBelowSearchField) {
  QuickInsertKeyEventHandler key_event_handler;
  QuickInsertPerformanceMetrics metrics;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* container =
      widget->SetContentsView(std::make_unique<QuickInsertMainContainerView>());

  auto* search_field = container->AddSearchFieldView(
      std::make_unique<QuickInsertSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  QuickInsertContentsView* contents = container->AddContentsView(
      QuickInsertLayoutType::kMainResultsBelowSearchField);

  EXPECT_GE(contents->GetBoundsInScreen().y(),
            search_field->GetBoundsInScreen().bottom());
}

TEST_F(QuickInsertMainContainerViewTest, LayoutWithContentsAboveSearchField) {
  QuickInsertKeyEventHandler key_event_handler;
  QuickInsertPerformanceMetrics metrics;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* container =
      widget->SetContentsView(std::make_unique<QuickInsertMainContainerView>());

  auto* search_field = container->AddSearchFieldView(
      std::make_unique<QuickInsertSearchFieldView>(
          base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  QuickInsertContentsView* contents = container->AddContentsView(
      QuickInsertLayoutType::kMainResultsAboveSearchField);

  EXPECT_LE(contents->GetBoundsInScreen().bottom(),
            search_field->GetBoundsInScreen().y());
}

TEST_F(QuickInsertMainContainerViewTest, SetsActivePage) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* container =
      widget->SetContentsView(std::make_unique<QuickInsertMainContainerView>());
  container->AddContentsView(
      QuickInsertLayoutType::kMainResultsBelowSearchField);
  auto* page1 = container->AddPage(std::make_unique<MockQuickInsertPageView>());
  auto* page2 = container->AddPage(std::make_unique<MockQuickInsertPageView>());

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
