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
    for (int j = 0; j < kNumColumns; ++j) {
      DesksTemplatesItemView* grid_item =
          layout->AddView(std::make_unique<DesksTemplatesItemView>());
      grid_items_.push_back(grid_item);
    }
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

void DesksTemplatesGridView::OnMouseEvent(ui::MouseEvent* event) {
  OnLocatedEvent(event, /*is_touch=*/false);
}

void DesksTemplatesGridView::OnGestureEvent(ui::GestureEvent* event) {
  OnLocatedEvent(event, /*is_touch=*/true);
}

void DesksTemplatesGridView::AddedToWidget() {
  // Adding a pre-target handler to ensure that events are not accidentally
  // captured by the child views. Also, `this` is added as the pre-target
  // handler to the window as opposed to `Env` to ensure that we only get events
  // that are on this window.
  widget_window_ = GetWidget()->GetNativeWindow();
  widget_window_->AddPreTargetHandler(this);
}

void DesksTemplatesGridView::RemovedFromWidget() {
  DCHECK(widget_window_);
  widget_window_->RemovePreTargetHandler(this);
  widget_window_ = nullptr;
}

void DesksTemplatesGridView::OnLocatedEvent(ui::LocatedEvent* event,
                                            bool is_touch) {
  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP:
      for (auto* grid_item : grid_items_)
        grid_item->UpdateDeleteButtonVisibility();
      return;
    default:
      return;
  }
}

BEGIN_METADATA(DesksTemplatesGridView, views::View)
END_METADATA

}  // namespace ash
