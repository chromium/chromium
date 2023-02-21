// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_MENU_MODEL_ADAPTER_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_MENU_MODEL_ADAPTER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_model_adapter.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class MenuItemView;
class MenuRunner;
}  // namespace views

namespace ash {

namespace clipboard_history_util {
enum class Action;
}  // namespace clipboard_history_util

class ClipboardHistory;
class ClipboardHistoryItem;
class ClipboardHistoryItemView;
class ClipboardHistoryResourceManager;

// Used to show the clipboard history menu, which holds the last few things
// copied.
class ASH_EXPORT ClipboardHistoryMenuModelAdapter
    : public views::MenuModelAdapter {
 public:
  static std::unique_ptr<ClipboardHistoryMenuModelAdapter> Create(
      ui::SimpleMenuModel::Delegate* delegate,
      base::RepeatingClosure menu_closed_callback,
      const ClipboardHistory* clipboard_history,
      const ClipboardHistoryResourceManager* resource_manager);

  ClipboardHistoryMenuModelAdapter(const ClipboardHistoryMenuModelAdapter&) =
      delete;
  ClipboardHistoryMenuModelAdapter& operator=(
      const ClipboardHistoryMenuModelAdapter&) = delete;
  ~ClipboardHistoryMenuModelAdapter() override;

  // Shows the menu anchored at `anchor_rect`, `source_type` indicates how the
  // menu is triggered.
  void Run(const gfx::Rect& anchor_rect,
           ui::MenuSourceType source_type);

  // Returns if the menu is currently running.
  bool IsRunning() const;

  // Hides and cancels the menu.
  void Cancel();

  // Returns the command of the currently selected menu item. If no menu item is
  // currently selected, returns |absl::nullopt|.
  absl::optional<int> GetSelectedMenuItemCommand() const;

  // Returns the item mapped by `command_id` in `item_snapshots_`.
  const ClipboardHistoryItem& GetItemFromCommandId(int command_id) const;

  // Returns the count of menu items.
  size_t GetMenuItemsCount() const;

  // Selects the menu item specified by `command_id`.
  void SelectMenuItemWithCommandId(int command_id);

  // Selects the menu item hovered by mouse.
  void SelectMenuItemHoveredByMouse();

  // Removes the menu item specified by `command_id`.
  void RemoveMenuItemWithCommandId(int command_id);

  // Advances the pseudo focus (backward if `reverse` is true).
  void AdvancePseudoFocus(bool reverse);

  // Returns the action to take on the menu item specified by `command_id`.
  clipboard_history_util::Action GetActionForCommandId(int command_id) const;

  // Returns menu bounds in screen coordinates.
  gfx::Rect GetMenuBoundsInScreenForTest() const;

  const views::MenuItemView* GetMenuItemViewAtForTest(size_t index) const;
  views::MenuItemView* GetMenuItemViewAtForTest(size_t index);

 private:
  class ScopedA11yIgnore;

  using ItemViewsByCommandId = std::map<int, ClipboardHistoryItemView*>;

  ClipboardHistoryMenuModelAdapter(
      std::unique_ptr<ui::SimpleMenuModel> model,
      base::RepeatingClosure menu_closed_callback,
      const ClipboardHistory* clipboard_history,
      const ClipboardHistoryResourceManager* resource_manager);

  // Advances the pseduo focus from the selected history item view (backward if
  // `reverse` is true).
  void AdvancePseudoFocusFromSelectedItem(bool reverse);

  // Returns the command id of the menu item to be selected after the
  // menu item specified by `command_id` is deleted.
  int CalculateSelectedCommandIdAfterDeletion(int command_id) const;

  // Removes the item view specified by `command_id` from the root menu.
  void RemoveItemView(int command_id);

  // views::MenuModelAdapter:
  views::MenuItemView* AppendMenuItem(views::MenuItemView* menu,
                                      ui::MenuModel* model,
                                      size_t index) override;
  void OnMenuClosed(views::MenuItemView* menu) override;

  // The model which holds the contents of the menu.
  std::unique_ptr<ui::SimpleMenuModel> const model_;
  // The root MenuItemView which contains all child MenuItemViews. Owned by
  // |menu_runner_|.
  views::MenuItemView* root_view_ = nullptr;
  // Responsible for showing |root_view_|.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The timestamp taken when the menu is opened. Used in metrics.
  base::TimeTicks menu_open_time_;

  // The mapping between the command ids and items that are copied from
  // `clipboard_history_` when the menu is created. It is used to solve the
  // possible inconsistency between the menu model data and the clipboard
  // history data. For example, a new item is added to `clipboard_history_`
  // while the menu is showing.
  // It updates synchronously when a item is removed.
  std::map<int, ClipboardHistoryItem> item_snapshots_;

  // Stores mappings between command ids and history item view pointers.
  // It updates synchronously when a item is removed.
  ItemViewsByCommandId item_views_by_command_id_;

  const ClipboardHistory* const clipboard_history_;

  // Resource manager used to fetch image models. Owned by
  // ClipboardHistoryController.
  const ClipboardHistoryResourceManager* const resource_manager_;

  // Indicates the number of item deletion operations in progress. Note that
  // a `ClipboardHistoryItemView` instance is deleted asynchronously.
  int item_deletion_in_progress_count_ = 0;

  std::unique_ptr<ScopedA11yIgnore> scoped_ignore_;

  // Indicates whether `Run()` has been called before.
  bool run_before_ = false;

  base::WeakPtrFactory<ClipboardHistoryMenuModelAdapter> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_MENU_MODEL_ADAPTER_H_
