// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_name_view.h"

#include <memory>

#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/background.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/native_cursor.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

bool IsDesksBarWidget(const views::Widget* widget) {
  if (!widget)
    return false;

  auto* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return false;

  auto* session = overview_controller->overview_session();
  for (const auto& grid : session->grid_list()) {
    if (widget == grid->desks_widget())
      return true;
  }

  return false;
}

}  // namespace

DeskNameView::DeskNameView(DeskMiniView* mini_view) : mini_view_(mini_view) {}

DeskNameView::~DeskNameView() = default;

// static
constexpr size_t DeskNameView::kMaxLength;

// static
void DeskNameView::CommitChanges(views::Widget* widget) {
  DCHECK(IsDesksBarWidget(widget));

  auto* focus_manager = widget->GetFocusManager();
  focus_manager->ClearFocus();
  // Avoid having the focus restored to the same DeskNameView when the desks bar
  // widget is refocused, e.g. when the new desk button is pressed.
  focus_manager->SetStoredFocusView(nullptr);
}

bool DeskNameView::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  // The default behavior of the tab key is that it moves the focus to the next
  // available DeskNameView.
  // We want that to be handled by OverviewHighlightController as part of moving
  // the highlight forward or backward when tab or shift+tab are pressed.
  if (event.key_code() == ui::VKEY_TAB)
    return true;

  return false;
}

void DeskNameView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Textfield::GetAccessibleNodeData(node_data);
  // When Bento is enabled and the user creates a new desk, |full_text_| will be
  // empty but the accesssible name for |this| will be the default desk name.
  node_data->SetName(full_text_.empty() ? GetAccessibleName() : full_text_);
}

views::View* DeskNameView::GetView() {
  return this;
}

void DeskNameView::MaybeActivateHighlightedView() {
  RequestFocus();
}

void DeskNameView::MaybeCloseHighlightedView() {}

void DeskNameView::MaybeSwapHighlightedView(bool right) {}

void DeskNameView::OnViewHighlighted() {
  UpdateBorderState();
  mini_view_->owner_bar()->ScrollToShowMiniViewIfNecessary(mini_view_);
}

void DeskNameView::OnViewUnhighlighted() {
  UpdateBorderState();
}

void DeskNameView::UpdateBorderState() {
  border_ptr_->SetFocused(IsViewHighlighted() || HasFocus());
  SchedulePaint();
}

BEGIN_METADATA(DeskNameView, LabelTextfield)
END_METADATA

}  // namespace ash
