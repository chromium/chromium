// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_widget.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr int kPanelDefaultWidth = 340;
constexpr int kPanelDefaultHeight = 450;
constexpr int kPanelBoundsShelfPadding = 8;

using MahiPanelWidgetTest = AshTestBase;

TEST_F(MahiPanelWidgetTest, WidgetBounds) {
  auto* root_window = GetContext();
  auto widget = MahiPanelWidget::CreatePanelWidget(GetPrimaryDisplay().id());

  auto bottom_right = root_window->bounds().bottom_right();
  EXPECT_EQ(
      gfx::Rect(
          bottom_right.x() - kPanelDefaultWidth - kPanelBoundsShelfPadding,
          bottom_right.y() - kPanelDefaultHeight -
              ShelfConfig::Get()->shelf_size() - kPanelBoundsShelfPadding,
          kPanelDefaultWidth, kPanelDefaultHeight),
      widget->GetRestoredBounds());
}

}  // namespace
}  // namespace ash
