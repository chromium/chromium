// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_grid_view.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/desks/templates/desks_templates_item_view.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The column set id that this view's GridLayout uses.
constexpr int kColumnSetId = 0;

constexpr int kNumColumns = 3;
constexpr int kNumRows = 2;

// TODO(richui): Replace these temporary values once specs come out.
constexpr int kGridPaddingDp = 25;

}  // namespace

DesksTemplatesGridView::DesksTemplatesGridView() {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);

  // Add `kNumColumns` and some padding between each one.
  const float fixed_size = views::GridLayout::kFixedSize;
  for (int i = 0; i < kNumColumns; ++i) {
    // Add a padding column in front of each column except the first one.
    if (i != 0)
      column_set->AddPaddingColumn(fixed_size, kGridPaddingDp);

    column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                          fixed_size,
                          views::GridLayout::ColumnSize::kUsePreferred,
                          /*fixed_width=*/0, /*min_width=*/0);
  }

  // Add a placeholder view in each available slot on the grid.
  // TODO(richui): Add a new class which shows the preview for each
  // template. These should also be only created as needed.
  for (int i = 0; i < kNumRows; ++i) {
    // Add padding in front of each row except the first one.
    if (i == 0) {
      layout->StartRow(fixed_size, kColumnSetId);
    } else {
      layout->StartRowWithPadding(fixed_size, kColumnSetId, fixed_size,
                                  kGridPaddingDp);
    }

    for (int j = 0; j < kNumColumns; ++j)
      layout->AddView(std::make_unique<DesksTemplatesItemView>());
  }
}

DesksTemplatesGridView::~DesksTemplatesGridView() = default;

// static
views::UniqueWidgetPtr DesksTemplatesGridView::CreateDesksTemplatesGridWidget(
    aura::Window* root,
    const gfx::Rect& grid_bounds) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = true;
  // The parent should be a container that covers all the windows but is below
  // some other system UI features such as system tray and capture mode.
  // TODO(sammiequon): It is possible but unlikely due to the bounds we choose
  // that this can cover the shelf. Investigate if there is a more suitable
  // container for this widget.
  params.parent = root->GetChildById(kShellWindowId_ShelfContainer);
  params.name = "DesksTemplatesGridWidget";

  views::UniqueWidgetPtr widget(
      std::make_unique<views::Widget>(std::move(params)));
  auto* desks_template_grid_view =
      widget->SetContentsView(std::make_unique<DesksTemplatesGridView>());
  gfx::Rect widget_bounds(grid_bounds);
  widget_bounds.ClampToCenteredSize(
      desks_template_grid_view->GetPreferredSize());
  widget->SetBounds(widget_bounds);

  // Not opaque since we want to view the contents of the layer behind.
  widget->GetLayer()->SetFillsBoundsOpaquely(false);

  return widget;
}

BEGIN_METADATA(DesksTemplatesGridView, views::View)
END_METADATA

}  // namespace ash
