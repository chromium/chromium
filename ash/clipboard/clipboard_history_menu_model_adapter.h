// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_MENU_MODEL_ADAPTER_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_MENU_MODEL_ADAPTER_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
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

// Used to show the clipboard history menu, which holds the last few things
// copied.
class ASH_EXPORT ClipboardHistoryMenuModelAdapter
    : public views::MenuModelAdapter {
 public:
  static std::unique_ptr<ClipboardHistoryMenuModelAdapter> Create(
      ui::SimpleMenuModel::Delegate* delegate,
      ClipboardHistoryController::OnMenuClosingCallback
          on_menu_closing_callback,
      base::RepeatingClosure menu_closed_callback,
      const ClipboardHistory* clipboard_history);

  ClipboardHistoryMenuModelAdapter(const ClipboardHistoryMenuModelAdapter&) =
      delete;
  ClipboardHistoryMenuModelAdapter& operator=(
      const ClipboardHistoryMenuModelAdapter&) = delete;
  ~ClipboardHistoryMenuModelAdapter() override;

  // Shows the menu anchored at `anchor_rect`. `source_type` and `show_source`
  // indicate how the menu was triggered. `menu_last_time_shown` and
  // `nudge_last_time_shown` indicate when the menu or any nudge was last shown.
  void Run(const gfx::Rect& anchor_rect,
           ui::MenuSourceType source_type,
           crosapi::mojom::ClipboardHistoryControllerShowSource show_source,
           const std::optional<base::Time>& menu_last_time_shown,
           const std::optional<base::Time>& nudge_last_time_shown);

  // Returns if the menu is currently running.
  bool IsRunning() const;

  // Hides and cancels the menu. `will_paste_item` indicates whether a clipboard
  // history item will be pasted after the menu is closed.
  void Cancel(bool will_paste_item);

  // Returns the command of the menu's first clipboard history item. This
  // differs from `clipboard_history_util::kFirstItemCommandId` when the menu's
  // first item has been removed. If the menu is empty, the result is absent.
  std::optional<int> GetFirstMenuItemCommand();

  // Returns the command of the currently selected menu item. If no menu item is
  // currently selected, returns |std::nullopt|.
  std::optional<int> GetSelectedMenuItemCommand() const;

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

  const ui::SimpleMenuModel* GetModelForTest() const;

 private:
  class MenuModelWithWillCloseCallback;
  class ScopedA11yIgnore;

  using ItemViewsByCommandId =
      std::map<int, raw_ptr<ClipboardHistoryItemView, CtnExperimental>>;

  ClipboardHistoryMenuModelAdapter(
      std::unique_ptr<MenuModelWithWillCloseCallback> model,
      base::RepeatingClosure menu_closed_callback,
      const ClipboardHistory* clipboard_history);

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
  std::unique_ptr<MenuModelWithWillCloseCallback> const model_;

  // Responsible for showing `root_view_`.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The root MenuItemView which contains all child MenuItemViews. Owned by
  // `menu_runner_`.
  raw_ptr<views::MenuItemView> root_view_ = nullptr;

  // The timestamp taken when the menu is opened. Used in metrics.
  base::TimeTicks menu_open_time_;

  // The source which opened the menu, absent until the menu is `Run()`.
  std::optional<crosapi::mojom::ClipboardHistoryControllerShowSource>
      menu_show_source_;

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

  const raw_ptr<const ClipboardHistory> clipboard_history_;

  // Indicates the number of item deletion operations in progress. Note that
  // a `ClipboardHistoryItemView` instance is deleted asynchronously.
  int item_deletion_in_progress_count_ = 0;

  // The index of the clipboard history menu header, if it exists.
  std::optional<size_t> header_index_;

  // The index of the clipboard history menu footer, if it exists.
  std::optional<size_t> footer_index_;

  std::unique_ptr<ScopedA11yIgnore> scoped_ignore_;

  // Indicates whether `Run()` has been called before.
  bool run_before_ = false;

  base::WeakPtrFactory<ClipboardHistoryMenuModelAdapter> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_MENU_MODEL_ADAPTER_H_
