// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_grid_view.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/desks/templates/desks_templates_item_view.h"
#include "ash/wm/desks/templates/desks_templates_presenter.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_handler.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The column set id that this view's GridLayout uses.
constexpr int kColumnSetId = 0;

constexpr int kNumColumns = 3;

// TODO(richui): Replace these temporary values once specs come out.
constexpr int kGridPaddingDp = 25;

}  // namespace

// -----------------------------------------------------------------------------
// DesksTemplatesEventHandler:

// This class is owned by DesksTemplatesGridView for the purpose of handling
// mouse and gesture events.
class DesksTemplatesEventHandler : public ui::EventHandler {
 public:
  explicit DesksTemplatesEventHandler(DesksTemplatesGridView* owner)
      : owner_(owner) {}
  DesksTemplatesEventHandler(const DesksTemplatesEventHandler&) = delete;
  DesksTemplatesEventHandler& operator=(const DesksTemplatesEventHandler&) =
      delete;
  ~DesksTemplatesEventHandler() override = default;

  void OnMouseEvent(ui::MouseEvent* event) override {
    owner_->OnLocatedEvent(event, /*is_touch=*/false);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    owner_->OnLocatedEvent(event, /*is_touch=*/true);
  }

 private:
  DesksTemplatesGridView* const owner_;
};

// -----------------------------------------------------------------------------
// DesksTemplatesGridView:

DesksTemplatesGridView::DesksTemplatesGridView() = default;

DesksTemplatesGridView::~DesksTemplatesGridView() = default;

// static
views::UniqueWidgetPtr DesksTemplatesGridView::CreateDesksTemplatesGridWidget(
    aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = true;
  // The parent should be a container that covers all the windows but is below
  // some other system UI features such as system tray and capture mode and also
  // below the system modal dialogs.
  // TODO(sammiequon): Investigate if there is a more suitable container for
  // this widget.
  params.parent = root->GetChildById(kShellWindowId_ShelfBubbleContainer);
  params.name = "DesksTemplatesGridWidget";

  views::UniqueWidgetPtr widget(
      std::make_unique<views::Widget>(std::move(params)));
  widget->SetContentsView(std::make_unique<DesksTemplatesGridView>());

  // Not opaque since we want to view the contents of the layer behind.
  widget->GetLayer()->SetFillsBoundsOpaquely(false);

  return widget;
}

void DesksTemplatesGridView::UpdateGridUI(
    const std::vector<DeskTemplate*>& desk_templates,
    const gfx::Rect& grid_bounds) {
  RemoveAllChildViews();
  grid_items_.clear();

  if (desk_templates.empty())
    return;

  DCHECK_LE(desk_templates.size(),
            DesksTemplatesPresenter::Get()->GetMaxEntryCount());

  layout_ = SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* column_set = layout_->AddColumnSet(kColumnSetId);

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

  // Add each of the templates to the grid.
  for (size_t i = 0; i < desk_templates.size(); ++i) {
    // Add padding in front of each row except the first one.
    if (i == 0) {
      layout_->StartRow(fixed_size, kColumnSetId);
    } else if (i % kNumColumns == 0) {
      layout_->StartRowWithPadding(fixed_size, kColumnSetId, fixed_size,
                                   kGridPaddingDp);
    }
    grid_items_.push_back(layout_->AddView(
        std::make_unique<DesksTemplatesItemView>(desk_templates[i])));
  }

  gfx::Rect widget_bounds(grid_bounds);
  widget_bounds.ClampToCenteredSize(GetPreferredSize());
  GetWidget()->SetBounds(widget_bounds);
}

void DesksTemplatesGridView::AddedToWidget() {
  // Adding a pre-target handler to ensure that events are not accidentally
  // captured by the child views. Also, an `EventHandler`
  // (DesksTemplatesEventHandler) is added as the pre-target handler to the
  // window as opposed to `Env` to ensure that we only get events that are on
  // this window.
  event_handler_ = std::make_unique<DesksTemplatesEventHandler>(this);
  widget_window_ = GetWidget()->GetNativeWindow();
  widget_window_->AddPreTargetHandler(event_handler_.get());
}

void DesksTemplatesGridView::RemovedFromWidget() {
  DCHECK(event_handler_);
  DCHECK(widget_window_);
  widget_window_->RemovePreTargetHandler(event_handler_.get());
  event_handler_.reset();
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
        grid_item->UpdateHoverButtonsVisibility();
      return;
    default:
      return;
  }
}

BEGIN_METADATA(DesksTemplatesGridView, views::View)
END_METADATA

}  // namespace ash
