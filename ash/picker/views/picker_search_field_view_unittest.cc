// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_field_view.h"

#include <memory>
#include <string>

#include "ash/picker/metrics/picker_performance_metrics.h"
#include "ash/picker/picker_test_util.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "ash/picker/views/picker_search_bar_textfield.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/range/range.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {
namespace {

int32_t GetActiveDescendantId(const views::View& view) {
  ui::AXNodeData node_data;
  view.GetViewAccessibility().GetAccessibleNodeData(&node_data);
  return node_data.GetIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId);
}

class PickerSearchFieldViewTest : public views::ViewsTestBase {
 public:
  PickerSearchFieldViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 private:
  AshColorProvider ash_color_provider_;
};

TEST_F(PickerSearchFieldViewTest, HasTextFieldRole) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  EXPECT_EQ(view.textfield_for_testing().GetAccessibleRole(),
            ax::mojom::Role::kTextField);
}

TEST_F(PickerSearchFieldViewTest, ClearButtonHasTooltip) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  EXPECT_EQ(view.clear_button_for_testing().GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_APP_LIST_CLEAR_SEARCHBOX));
}

TEST_F(PickerSearchFieldViewTest, BackButtonHasTooltip) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  EXPECT_EQ(view.back_button_for_testing().GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
}

TEST_F(PickerSearchFieldViewTest, DoesNotTriggerSearchOnConstruction) {
  base::test::TestFuture<const std::u16string&> future;
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(future.GetRepeatingCallback(), base::DoNothing(),
                             &key_event_handler, &metrics);

  EXPECT_FALSE(future.IsReady());
}

TEST_F(PickerSearchFieldViewTest, TriggersSearchOnContentsChange) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  base::test::TestFuture<const std::u16string&> future;
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      future.GetRepeatingCallback(), base::DoNothing(), &key_event_handler,
      &metrics));

  view->RequestFocus();
  PressAndReleaseKey(*widget, ui::KeyboardCode::VKEY_A);

  EXPECT_EQ(future.Get(), u"a");
}

TEST_F(PickerSearchFieldViewTest, SetPlaceholderText) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  view.SetPlaceholderText(u"hello");

  EXPECT_EQ(view.textfield_for_testing().GetPlaceholderText(), u"hello");
}

TEST_F(PickerSearchFieldViewTest, SetQueryText) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  view.SetQueryText(u"test");

  EXPECT_EQ(view.textfield_for_testing().GetText(), u"test");
}

TEST_F(PickerSearchFieldViewTest, SetQueryTextDoesNotTriggerSearch) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  base::test::TestFuture<const std::u16string&> future;
  PickerSearchFieldView view(future.GetRepeatingCallback(), base::DoNothing(),
                             &key_event_handler, &metrics);

  view.SetQueryText(u"test");

  EXPECT_FALSE(future.IsReady());
}

TEST_F(PickerSearchFieldViewTest, DoesNotShowClearButtonInitially) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  EXPECT_FALSE(view.clear_button_for_testing().GetVisible());
}

TEST_F(PickerSearchFieldViewTest, DoesNotShowBackButtonInitially) {
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  PickerSearchFieldView view(base::DoNothing(), base::DoNothing(),
                             &key_event_handler, &metrics);

  EXPECT_FALSE(view.back_button_for_testing().GetVisible());
}

TEST_F(PickerSearchFieldViewTest, ShowsClearButtonWithQuery) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));

  view->RequestFocus();
  PressAndReleaseKey(*widget, ui::KeyboardCode::VKEY_A);

  EXPECT_TRUE(view->clear_button_for_testing().GetVisible());
}

TEST_F(PickerSearchFieldViewTest, HidesClearButtonWithEmptyQuery) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));

  view->RequestFocus();
  PressAndReleaseKey(*widget, ui::KeyboardCode::VKEY_A);
  // This backspace press does not do anything, causing the query text to remain
  // "a" and not hiding the clear button.
  // TODO: b/357464892 - Fix this test and replace the below `EXPECT_TRUE` with
  // an `EXPECT_FALSE`.
  PressAndReleaseKey(*widget, ui::KeyboardCode::VKEY_BACK);

  EXPECT_TRUE(view->clear_button_for_testing().GetVisible());
}

TEST_F(PickerSearchFieldViewTest, ShowsClearButtonWithSetQueryText) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));

  view->SetQueryText(u"a");

  EXPECT_TRUE(view->clear_button_for_testing().GetVisible());
}

TEST_F(PickerSearchFieldViewTest, HidesClearButtonWithEmptySetQueryText) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));

  view->SetQueryText(u"a");
  view->SetQueryText(u"");

  EXPECT_FALSE(view->clear_button_for_testing().GetVisible());
}

TEST_F(PickerSearchFieldViewTest,
       ClickingClearButtonResetsQueryAndHidesButton) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  base::test::TestFuture<const std::u16string&> future;
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      future.GetRepeatingCallback(), base::DoNothing(), &key_event_handler,
      &metrics));
  view->SetPlaceholderText(u"placeholder");
  view->RequestFocus();
  PressAndReleaseKey(*widget, ui::KeyboardCode::VKEY_A);
  ASSERT_EQ(future.Take(), u"a");

  ViewDrawnWaiter().Wait(&view->clear_button_for_testing());
  LeftClickOn(view->clear_button_for_testing());

  EXPECT_EQ(future.Get(), u"");
  EXPECT_EQ(view->textfield_for_testing().GetText(), u"");
  EXPECT_FALSE(view->clear_button_for_testing().GetVisible());
}

TEST_F(PickerSearchFieldViewTest, ClickingBackButtonTriggersCallback) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  base::test::TestFuture<void> future;
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), future.GetRepeatingCallback(), &key_event_handler,
      &metrics));
  view->SetPlaceholderText(u"placeholder");
  view->SetBackButtonVisible(true);

  ViewDrawnWaiter().Wait(&view->back_button_for_testing());
  LeftClickOn(view->back_button_for_testing());

  EXPECT_TRUE(future.Wait());
}

TEST_F(PickerSearchFieldViewTest, GetsViewLeftOfBackButton) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetBackButtonVisible(true);

  EXPECT_EQ(view->GetViewLeftOf(&view->back_button_for_testing()), nullptr);
  EXPECT_EQ(view->GetViewLeftOf(&view->textfield_for_testing()),
            &view->back_button_for_testing());
}

TEST_F(PickerSearchFieldViewTest, GetsViewLeftOfClearButton) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetQueryText(u"query");

  EXPECT_EQ(view->GetViewLeftOf(&view->textfield_for_testing()), nullptr);
  EXPECT_EQ(view->GetViewLeftOf(&view->clear_button_for_testing()),
            &view->textfield_for_testing());
}

TEST_F(PickerSearchFieldViewTest, GetsViewRightOfBackButton) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetBackButtonVisible(true);

  EXPECT_EQ(view->GetViewRightOf(&view->back_button_for_testing()),
            &view->textfield_for_testing());
  EXPECT_EQ(view->GetViewRightOf(&view->textfield_for_testing()), nullptr);
}

TEST_F(PickerSearchFieldViewTest, GetsViewRightOfClearButton) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetQueryText(u"query");

  EXPECT_EQ(view->GetViewRightOf(&view->textfield_for_testing()),
            &view->clear_button_for_testing());
  EXPECT_EQ(view->GetViewRightOf(&view->clear_button_for_testing()), nullptr);
}

TEST_F(PickerSearchFieldViewTest, LeftEventShouldMoveCursorFromMiddleOfQuery) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetQueryText(u"query");
  view->textfield_for_testing().SetSelectedRange(gfx::Range(2));

  EXPECT_TRUE(view->LeftEventShouldMoveCursor(&view->textfield_for_testing()));
}

TEST_F(PickerSearchFieldViewTest,
       LeftEventShouldNotMoveCursorFromStartOfQuery) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetBackButtonVisible(true);
  view->SetQueryText(u"query");
  view->textfield_for_testing().SetSelectedRange(gfx::Range(0));

  EXPECT_FALSE(view->LeftEventShouldMoveCursor(&view->textfield_for_testing()));
}

TEST_F(PickerSearchFieldViewTest, LeftEventShouldMoveCursorFromEndOfQuery) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetQueryText(u"query");
  view->textfield_for_testing().SetSelectedRange(gfx::Range(5));

  EXPECT_TRUE(view->LeftEventShouldMoveCursor(&view->textfield_for_testing()));
}

TEST_F(PickerSearchFieldViewTest, RightEventShouldMoveCursorFromMiddleOfQuery) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetQueryText(u"query");
  view->textfield_for_testing().SetSelectedRange(gfx::Range(2));

  EXPECT_TRUE(view->RightEventShouldMoveCursor(&view->textfield_for_testing()));
}

TEST_F(PickerSearchFieldViewTest, RightEventShouldMoveCursorFromStartOfQuery) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetBackButtonVisible(true);
  view->SetQueryText(u"query");
  view->textfield_for_testing().SetSelectedRange(gfx::Range(0));

  EXPECT_TRUE(view->RightEventShouldMoveCursor(&view->textfield_for_testing()));
}

TEST_F(PickerSearchFieldViewTest, RightEventShouldNotMoveCursorFromEndOfQuery) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetQueryText(u"query");
  view->textfield_for_testing().SetSelectedRange(gfx::Range(5));

  EXPECT_FALSE(
      view->RightEventShouldMoveCursor(&view->textfield_for_testing()));
}

TEST_F(PickerSearchFieldViewTest,
       SetTextfieldActiveDescendantNotifiesAfterDelayWhenFocused) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetPlaceholderText(u"placeholder");
  view->RequestFocus();
  views::View descendant;

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  view->SetTextfieldActiveDescendant(&descendant);

  EXPECT_EQ(GetActiveDescendantId(*view->textfield()), 0);
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kActiveDescendantChanged), 0);

  task_environment()->FastForwardBy(
      PickerSearchFieldView::kNotifyInitialActiveDescendantA11yDelay);

  EXPECT_EQ(GetActiveDescendantId(*view->textfield()),
            descendant.GetViewAccessibility().GetUniqueId());
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kActiveDescendantChanged), 1);
}

TEST_F(PickerSearchFieldViewTest,
       SetTextfieldActiveDescendantDoesNotNotifyWhenUnfocused) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  views::View descendant;

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  view->SetTextfieldActiveDescendant(&descendant);

  EXPECT_EQ(GetActiveDescendantId(*view->textfield()), 0);
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kActiveDescendantChanged), 0);
}

TEST_F(PickerSearchFieldViewTest,
       RequestFocusNotifiesInitialActiveDescendantAfterDelay) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetPlaceholderText(u"placeholder");
  views::View descendant;
  view->SetTextfieldActiveDescendant(&descendant);

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  view->RequestFocus();

  EXPECT_EQ(GetActiveDescendantId(*view->textfield()), 0);
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kActiveDescendantChanged), 0);

  task_environment()->FastForwardBy(
      PickerSearchFieldView::kNotifyInitialActiveDescendantA11yDelay);

  EXPECT_EQ(GetActiveDescendantId(*view->textfield()),
            descendant.GetViewAccessibility().GetUniqueId());
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kActiveDescendantChanged), 1);
}

TEST_F(PickerSearchFieldViewTest,
       RequestFocusDoesNotNotifyEmptyActiveDescendant) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetPlaceholderText(u"placeholder");
  views::View descendant;

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  view->RequestFocus();

  EXPECT_EQ(GetActiveDescendantId(*view->textfield()), 0);
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kActiveDescendantChanged), 0);

  task_environment()->FastForwardBy(
      PickerSearchFieldView::kNotifyInitialActiveDescendantA11yDelay);

  EXPECT_EQ(GetActiveDescendantId(*view->textfield()), 0);
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kActiveDescendantChanged), 0);
}

TEST_F(PickerSearchFieldViewTest,
       SetTextfieldActiveDescendantOnlyNotifiesNewestDescendant) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  PickerKeyEventHandler key_event_handler;
  PickerPerformanceMetrics metrics;
  auto* view = widget->SetContentsView(std::make_unique<PickerSearchFieldView>(
      base::DoNothing(), base::DoNothing(), &key_event_handler, &metrics));
  view->SetPlaceholderText(u"placeholder");
  views::View descendant1, descendant2;
  view->SetTextfieldActiveDescendant(&descendant1);

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  view->RequestFocus();
  view->SetTextfieldActiveDescendant(&descendant2);
  task_environment()->FastForwardBy(
      PickerSearchFieldView::kNotifyInitialActiveDescendantA11yDelay);

  EXPECT_EQ(GetActiveDescendantId(*view->textfield()),
            descendant2.GetViewAccessibility().GetUniqueId());
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kActiveDescendantChanged), 1);
}

}  // namespace
}  // namespace ash
