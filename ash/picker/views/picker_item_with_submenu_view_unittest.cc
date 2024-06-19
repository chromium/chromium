// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_item_with_submenu_view.h"

#include <string>
#include <utility>

#include "ash/picker/views/picker_submenu_controller.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace {

class PickerItemWithSubmenuViewTest : public views::ViewsTestBase {
 private:
  AshColorProvider provider_;
};

TEST_F(PickerItemWithSubmenuViewTest, ShowsSubmenuOnEnter) {
  auto anchor_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  anchor_widget->SetContentsView(std::make_unique<views::View>());
  anchor_widget->Show();
  PickerSubmenuController submenu_controller;
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view =
      widget->SetContentsView(std::make_unique<PickerItemWithSubmenuView>());
  item_view->SetText(u"abc");
  item_view->SetSubmenuController(&submenu_controller);
  widget->Show();

  item_view->OnMouseEntered(ui::MouseEvent(
      ui::ET_MOUSE_MOVED, gfx::PointF(), gfx::PointF(),
      /*time_stamp=*/{}, ui::EF_IS_SYNTHESIZED, ui::EF_LEFT_MOUSE_BUTTON));

  views::test::WidgetVisibleWaiter(submenu_controller.widget_for_testing())
      .Wait();
}

}  // namespace
}  // namespace ash
