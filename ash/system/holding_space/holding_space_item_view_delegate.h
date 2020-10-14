// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_DELEGATE_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/scoped_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ui {
class GestureEvent;
class KeyEvent;
class MouseEvent;
}  // namespace ui

namespace views {
class MenuRunner;
}  // namespace views

namespace ash {

class HoldingSpaceItemView;

// A delegate for `HoldingSpaceItemView`s which implements context menu,
// drag-and-drop, and selection functionality. In order to support multiple
// selections at a time, all `HoldingSpaceItemView`s must share the same
// `HoldingSpaceItemViewDelegate` instance.
class ASH_EXPORT HoldingSpaceItemViewDelegate
    : public views::ContextMenuController,
      public views::DragController,
      public views::ViewObserver,
      public ui::SimpleMenuModel::Delegate {
 public:
  HoldingSpaceItemViewDelegate();
  HoldingSpaceItemViewDelegate(const HoldingSpaceItemViewDelegate&) = delete;
  HoldingSpaceItemViewDelegate& operator=(const HoldingSpaceItemViewDelegate&) =
      delete;
  ~HoldingSpaceItemViewDelegate() override;

  // Invoked when `view` has been created.
  void OnHoldingSpaceItemViewCreated(HoldingSpaceItemView* view);

  // Invoked when `view` should perform an accessible action. Returns true if
  // the action is handled, otherwise false.
  bool OnHoldingSpaceItemViewAccessibleAction(
      HoldingSpaceItemView* view,
      const ui::AXActionData& action_data);

  // Invoked when `view` receives the specified gesture `event`.
  void OnHoldingSpaceItemViewGestureEvent(HoldingSpaceItemView* view,
                                          const ui::GestureEvent& event);

  // Invoked when `view` receives the specified key pressed `event`.
  bool OnHoldingSpaceItemViewKeyPressed(HoldingSpaceItemView* view,
                                        const ui::KeyEvent& event);

  // Invoked when `view` receives the specified mouse pressed `event`.
  bool OnHoldingSpaceItemViewMousePressed(HoldingSpaceItemView* view,
                                          const ui::MouseEvent& event);

  // Invoked when `view` receives the specified mouse released `event`.
  void OnHoldingSpaceItemViewMouseReleased(HoldingSpaceItemView* view,
                                           const ui::MouseEvent& event);

 private:
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

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* view) override;

  // SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // Builds and returns a raw pointer to `context_menu_model_`.
  ui::SimpleMenuModel* BuildMenuModel();

  // Returns the subset of `views_` which are currently selected.
  std::vector<const HoldingSpaceItemView*> GetSelection();

  // Marks `view` as selected. All other `views_` are marked unselected.
  void SetSelection(views::View* view);

  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  std::vector<HoldingSpaceItemView*> views_;

  // Caches a view for which mouse released events should be temporarily
  // ignored. This is to prevent us from selecting a view on mouse pressed but
  // then unselecting that same view on mouse released.
  HoldingSpaceItemView* ignore_mouse_released_ = nullptr;

  // We observe `views_` for their lifetime so we can track selected state.
  ScopedObserver<views::View, views::ViewObserver> view_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_DELEGATE_H_
