// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_VIEW_DELEGATE_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_VIEW_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/drag_controller.h"

namespace ui {
class GestureEvent;
class MouseEvent;
}  // namespace ui

namespace views {
class MenuRunner;
}  // namespace views

namespace ash {
class SearchResultImageView;

// TODO(crbug.com/1352636) implement class functionality.
// A delegate for `SearchResultImageView` which implements context menu, drag
// and drop, and selection functionality. Only a single delegate instance
// exists at a time and is shared by all existing search result image views in
// order to support multiselection which requires a shared state.
class ASH_EXPORT SearchResultImageViewDelegate
    : public views::ContextMenuController,
      public views::DragController,
      public ui::SimpleMenuModel::Delegate {
 public:
  // Returns the singleton instance.
  static SearchResultImageViewDelegate* Get();

  SearchResultImageViewDelegate();
  SearchResultImageViewDelegate(const SearchResultImageViewDelegate&) = delete;
  SearchResultImageViewDelegate& operator=(
      const SearchResultImageViewDelegate&) = delete;
  ~SearchResultImageViewDelegate() override;

  // Invoked when `view` receives the specified gesture `event`.
  void HandleSearchResultImageViewGestureEvent(SearchResultImageView* view,
                                               const ui::GestureEvent& event);

  // Invoked when `view` receives the specified mouse pressed `event`.
  void HandleSearchResultImageViewMouseEvent(SearchResultImageView* view,
                                             const ui::MouseEvent& event);

  // Checks for an active `context_menu_runner_`.
  bool HasActiveContextMenu() const;

 private:
  // Builds and returns a raw pointer to `context_menu_model_`.
  ui::SimpleMenuModel* BuildMenuModel();

  // Called when the context menu is closed. Used as a callback for
  // `context_menu_runner_`.
  void OnMenuClosed();

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // views::DragController:
  bool CanStartDragForView(views::View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& current_pt) override;
  int GetDragOperationsForView(views::View* sender,
                               const gfx::Point& press_pt) override;
  void WriteDragDataForView(views::View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;

  // SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  base::WeakPtrFactory<SearchResultImageViewDelegate> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_VIEW_DELEGATE_H_
