// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_image_view_delegate.h"

#include <memory>

#include "ash/app_list/views/search_result_image_view.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {
namespace {
SearchResultImageViewDelegate* g_instance = nullptr;
}  // namespace

// static
SearchResultImageViewDelegate* SearchResultImageViewDelegate::Get() {
  return g_instance;
}

SearchResultImageViewDelegate::SearchResultImageViewDelegate() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

SearchResultImageViewDelegate::~SearchResultImageViewDelegate() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// TODO(crbug.com/1352636)
void SearchResultImageViewDelegate::HandleSearchResultImageViewGestureEvent(
    SearchResultImageView* view,
    const ui::GestureEvent& event) {}

// TODO(crbug.com/1352636)
void SearchResultImageViewDelegate::HandleSearchResultImageViewMouseEvent(
    SearchResultImageView* view,
    const ui::MouseEvent& event) {
  switch (event.type()) {
    case ui::ET_MOUSE_ENTERED:
      // TODO(crbug.com/1352636) Paint background highlight to indicate hover.
      break;
    case ui::ET_MOUSE_EXITED:
      // TODO(crbug.com/1352636) Remove hover background highlight.
      break;
    case ui::ET_MOUSE_PRESSED:
      // TODO(crbug.com/1352636) implement multi-result selection.
      break;
    case ui::ET_MOUSE_RELEASED:
      // TODO(crbug.com/1352636) implement multi-result selection.
      break;
    default:
      break;
  }
}

bool SearchResultImageViewDelegate::HasActiveContextMenu() const {
  return context_menu_runner_ && context_menu_runner_->IsRunning();
}

ui::SimpleMenuModel* SearchResultImageViewDelegate::BuildMenuModel() {
  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  // TODO(crbug.com/1352636) update with internationalized accessible name if we
  // launch this feature.
  context_menu_model_->AddTitle(u"Search Result Image View Context Menu");
  // TODO(crbug.com/1352636) AddItemWithStringId(command_id,string_id);
  return context_menu_model_.get();
}

void SearchResultImageViewDelegate::OnMenuClosed() {
  context_menu_model_.reset();
  context_menu_runner_.reset();
}

// TODO(crbug.com/1352636) Add context menu support.
void SearchResultImageViewDelegate::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (HasActiveContextMenu())
    return;

  int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                  views::MenuRunner::CONTEXT_MENU |
                  views::MenuRunner::FIXED_ANCHOR;

  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      BuildMenuModel(), run_types,
      base::BindRepeating(&SearchResultImageViewDelegate::OnMenuClosed,
                          weak_ptr_factory_.GetWeakPtr()));

  context_menu_runner_->RunMenuAt(
      source->GetWidget(), /*button_controller=*/nullptr,
      /*bounds=*/gfx::Rect(point, gfx::Size()),
      views::MenuAnchorPosition::kBubbleBottomRight, source_type);
}

// TODO(crbug.com/1352636) Add drag drop support.
bool SearchResultImageViewDelegate::CanStartDragForView(
    views::View* sender,
    const gfx::Point& press_pt,
    const gfx::Point& current_pt) {
  return false;
}

int SearchResultImageViewDelegate::GetDragOperationsForView(
    views::View* sender,
    const gfx::Point& press_pt) {
  return ui::DragDropTypes::DRAG_COPY;
}

// TODO(crbug.com/1352636) Add drag drop support.
void SearchResultImageViewDelegate::WriteDragDataForView(
    views::View* sender,
    const gfx::Point& press_pt,
    ui::OSExchangeData* data) {
  return;
}

// TODO(crbug.com/1352636) Add context menu support.
void SearchResultImageViewDelegate::ExecuteCommand(int command_id,
                                                   int event_flags) {
  return;
}

}  // namespace ash
