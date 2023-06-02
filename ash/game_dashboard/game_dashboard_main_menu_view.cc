// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_main_menu_view.h"

#include <memory>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kBubbleCornerRadius = 8;
// Padding between the edges of the menu and the elements.
constexpr int kPaddingWidth = 12;

}  // namespace

GameDashboardMainMenuView::GameDashboardMainMenuView(
    views::Widget* main_menu_button_widget) {
  DCHECK(main_menu_button_widget);
  set_corner_radius(kBubbleCornerRadius);
  set_close_on_deactivate(false);
  set_internal_name("GameDashboardMainMenuView");
  set_margins(gfx::Insets());
  set_parent_window(main_menu_button_widget->GetNativeWindow());
  SetAnchorView(main_menu_button_widget->GetContentsView());
  SetArrow(views::BubbleBorder::Arrow::NONE);
  SetButtons(ui::DIALOG_BUTTON_NONE);

  auto* layout = SetLayoutManager(std::make_unique<views::TableLayout>());
  layout->AddPaddingColumn(views::TableLayout::kFixedSize, kPaddingWidth)
      .AddColumn(views::LayoutAlignment::kStart,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, kPaddingWidth)
      .AddPaddingRow(views::TableLayout::kFixedSize, kPaddingWidth)
      .AddRows(1, views::TableLayout::kFixedSize)
      .AddPaddingRow(views::TableLayout::kFixedSize, kPaddingWidth);
}

GameDashboardMainMenuView::~GameDashboardMainMenuView() = default;

BEGIN_METADATA(GameDashboardMainMenuView, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ash
