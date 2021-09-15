// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/desks_templates/desks_templates_grid_view.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kColumnSetId = 0;
constexpr int kNumColumns = 3;
constexpr int kNumRows = 2;
constexpr int kGridPaddingDp = 10;

}  // namespace

DesksTemplatesGridView::DesksTemplatesGridView() {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);

  // Add `kNumColumns` and some padding between each one.
  const float horizontal_resize_percent = 10.f / kNumColumns;
  for (int i = 0; i < kNumColumns; ++i) {
    // Add a padding column in front of each column except the first one.
    if (i != 0)
      column_set->AddPaddingColumn(1, kGridPaddingDp);

    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                          horizontal_resize_percent,
                          views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  }

  // Add a placeholder view in each available slot on the grid.
  // TODO(sammiequon): Add a new class which shows the preview for each
  // template. These should also be only created as needed.
  const float vertical_resize_percent = 10.f / kNumRows;
  for (int i = 0; i < kNumRows; ++i) {
    layout->StartRow(vertical_resize_percent, 0);
    for (int j = 0; j < kNumColumns; ++j) {
      views::View* placeholder_template_view =
          layout->AddView(std::make_unique<views::View>());
      placeholder_template_view->SetBorder(
          views::CreateSolidBorder(/*thickness=*/2, SK_ColorGRAY));
    }
  }
}

DesksTemplatesGridView::~DesksTemplatesGridView() = default;

// static
views::UniqueWidgetPtr DesksTemplatesGridView::CreateDesksTemplatesGridWidget(
    aura::Window* root,
    const gfx::Rect& bounds) {
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
  params.bounds = bounds;
  params.name = "DesksTemplatesGridWidget";

  views::UniqueWidgetPtr widget(
      std::make_unique<views::Widget>(std::move(params)));
  widget->SetContentsView(std::make_unique<DesksTemplatesGridView>());
  return widget;
}

BEGIN_METADATA(DesksTemplatesGridView, views::View)
END_METADATA

}  // namespace ash
