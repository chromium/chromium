// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_item_with_submenu_view.h"

#include <string>
#include <utility>

#include "ash/picker/views/picker_submenu_controller.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {
namespace {

class PickerItemWithSubmenuViewTest : public views::ViewsTestBase {
 private:
  AshColorProvider provider_;
};

TEST_F(PickerItemWithSubmenuViewTest, HasAccessibilityAttributes) {
  PickerItemWithSubmenuView view;

  ui::AXNodeData data;
  view.GetViewAccessibility().GetAccessibleNodeData(&data);

  EXPECT_EQ(data.role, ax::mojom::Role::kPopUpButton);
  EXPECT_EQ(data.GetHasPopup(), ax::mojom::HasPopup::kMenu);
}

TEST_F(PickerItemWithSubmenuViewTest, ShowsSubmenu) {
  PickerSubmenuController submenu_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view =
      widget->SetContentsView(std::make_unique<PickerItemWithSubmenuView>());
  item_view->SetText(u"abc");
  item_view->SetSubmenuController(&submenu_controller);
  widget->Show();

  item_view->ShowSubmenu();

  views::test::WidgetVisibleWaiter(submenu_controller.widget_for_testing())
      .Wait();
}

TEST_F(PickerItemWithSubmenuViewTest, ShowsSubmenuOnMouseEnter) {
  PickerSubmenuController submenu_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view =
      widget->SetContentsView(std::make_unique<PickerItemWithSubmenuView>());
  item_view->SetText(u"abc");
  item_view->SetSubmenuController(&submenu_controller);
  widget->Show();

  item_view->OnMouseEntered(ui::MouseEvent(
      ui::EventType::kMouseMoved, gfx::PointF(), gfx::PointF(),
      /*time_stamp=*/{}, ui::EF_IS_SYNTHESIZED, ui::EF_LEFT_MOUSE_BUTTON));

  views::test::WidgetVisibleWaiter(submenu_controller.widget_for_testing())
      .Wait();
}

TEST_F(PickerItemWithSubmenuViewTest, ShowsSubmenuOnGestureTap) {
  PickerSubmenuController submenu_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view =
      widget->SetContentsView(std::make_unique<PickerItemWithSubmenuView>());
  item_view->SetText(u"abc");
  item_view->SetSubmenuController(&submenu_controller);
  widget->Show();
  ViewDrawnWaiter().Wait(item_view);

  ui::test::EventGenerator event_generator(GetRootWindow(widget.get()));
  event_generator.GestureTapAt(item_view->GetBoundsInScreen().CenterPoint());

  views::test::WidgetVisibleWaiter(submenu_controller.widget_for_testing())
      .Wait();
}

}  // namespace
}  // namespace ash
