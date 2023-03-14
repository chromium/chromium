// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_menu_model_adapter.h"

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_item_view.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

// ClipboardHistoryMenuModelAdapter::ScopedA11yIgnore --------------------------

// The scoped class to disable a11y for all items views.
class ClipboardHistoryMenuModelAdapter::ScopedA11yIgnore {
 public:
  explicit ScopedA11yIgnore(
      ClipboardHistoryMenuModelAdapter* menu_model_adapter)
      : menu_model_adapter_(menu_model_adapter) {
    SetIgnoreA11yForAllItemViews(true);
  }
  ~ScopedA11yIgnore() { SetIgnoreA11yForAllItemViews(false); }

 private:
  void SetIgnoreA11yForAllItemViews(bool ignore) {
    for (auto& item_view_command_id_pair :
         menu_model_adapter_->item_views_by_command_id_) {
      views::View* item_view = item_view_command_id_pair.second;
      item_view->GetViewAccessibility().OverrideIsIgnored(ignore);
    }
  }

  ClipboardHistoryMenuModelAdapter* const menu_model_adapter_;
};

// ClipboardHistoryMenuModelAdapter --------------------------------------------

// static
std::unique_ptr<ClipboardHistoryMenuModelAdapter>
ClipboardHistoryMenuModelAdapter::Create(
    ui::SimpleMenuModel::Delegate* delegate,
    base::RepeatingClosure menu_closed_callback,
    const ClipboardHistory* clipboard_history,
    const ClipboardHistoryResourceManager* resource_manager) {
  return base::WrapUnique(new ClipboardHistoryMenuModelAdapter(
      std::make_unique<ui::SimpleMenuModel>(delegate),
      std::move(menu_closed_callback), clipboard_history, resource_manager));
}

ClipboardHistoryMenuModelAdapter::~ClipboardHistoryMenuModelAdapter() = default;

void ClipboardHistoryMenuModelAdapter::Run(
    const gfx::Rect& anchor_rect,
    ui::MenuSourceType source_type) {
  DCHECK(!root_view_);
  DCHECK(model_);
  DCHECK(item_snapshots_.empty());
  DCHECK(item_views_by_command_id_.empty());

  // `Run()` should be called at most once for an instance.
  DCHECK(!run_before_);
  run_before_ = true;

  menu_open_time_ = base::TimeTicks::Now();

  int command_id = clipboard_history_util::kFirstItemCommandId;
  const auto& items = clipboard_history_->GetItems();
  // Do not include the final kDeleteCommandId item in histograms, because it
  // is not shown.
  UMA_HISTOGRAM_COUNTS_100(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", items.size());

  const ui::DataTransferEndpoint data_dst(ui::EndpointType::kDefault,
                                          /*notify_if_restricted=*/false);
  for (const auto& item : items) {
    model_->AddItem(command_id, std::u16string());
    item_snapshots_.emplace(command_id, item);
    ++command_id;
  }

  // Start async rendering of HTML, if any exists.
  ClipboardImageModelFactory::Get()->Activate();

  root_view_ = CreateMenu();
  root_view_->SetTitle(
      l10n_util::GetStringUTF16(IDS_CLIPBOARD_HISTORY_MENU_TITLE));
  menu_runner_ = std::make_unique<views::MenuRunner>(
      root_view_, views::MenuRunner::CONTEXT_MENU |
                      views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                      views::MenuRunner::FIXED_ANCHOR);
  menu_runner_->RunMenuAt(
      /*widget_owner=*/nullptr, /*menu_button_controller=*/nullptr, anchor_rect,
      views::MenuAnchorPosition::kBubbleBottomRight, source_type);
}

bool ClipboardHistoryMenuModelAdapter::IsRunning() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

void ClipboardHistoryMenuModelAdapter::Cancel() {
  DCHECK(menu_runner_);
  menu_runner_->Cancel();
}

absl::optional<int>
ClipboardHistoryMenuModelAdapter::GetSelectedMenuItemCommand() const {
  DCHECK(root_view_);

  // `root_view_` may be selected if no menu item is under selection.
  auto* menu_item = root_view_->GetMenuController()->GetSelectedMenuItem();
  return menu_item && menu_item != root_view_
             ? absl::make_optional(menu_item->GetCommand())
             : absl::nullopt;
}

const ClipboardHistoryItem&
ClipboardHistoryMenuModelAdapter::GetItemFromCommandId(int command_id) const {
  auto iter = item_snapshots_.find(command_id);
  DCHECK(iter != item_snapshots_.cend());
  return iter->second;
}

size_t ClipboardHistoryMenuModelAdapter::GetMenuItemsCount() const {
  // We should not use `root_view_` to retrieve the item count. Because the
  // menu item view is removed from `root_view_` asynchronously.
  return item_views_by_command_id_.size();
}

void ClipboardHistoryMenuModelAdapter::SelectMenuItemWithCommandId(
    int command_id) {
  views::MenuItemView* selected_menu_item =
      root_view_->GetMenuItemByID(command_id);
  DCHECK(IsRunning());
  views::MenuController::GetActiveInstance()->SelectItemAndOpenSubmenu(
      selected_menu_item);
}

void ClipboardHistoryMenuModelAdapter::SelectMenuItemHoveredByMouse() {
  // Find the menu item hovered by mouse.
  auto iter = base::ranges::find_if(item_views_by_command_id_,
                                    &views::View::IsMouseHovered,
                                    &ItemViewsByCommandId::value_type::second);

  if (iter == item_views_by_command_id_.cend()) {
    // If no item is hovered by mouse, cancel the selection on the child menu
    // item by selecting the root menu item.
    views::MenuController::GetActiveInstance()->SelectItemAndOpenSubmenu(
        root_view_);
  } else {
    SelectMenuItemWithCommandId(iter->first);
  }
}

void ClipboardHistoryMenuModelAdapter::RemoveMenuItemWithCommandId(
    int command_id) {
  // Calculate `new_selected_command_id` before removing the item specified by
  // `command_id` from data structures because the item to be removed is
  // needed in calculation.
  absl::optional<int> new_selected_command_id =
      CalculateSelectedCommandIdAfterDeletion(command_id);

  // Disable a11y for all item views. It ensures that when deleting multiple
  // item views, only the one finally selected is announced.
  if (!item_deletion_in_progress_count_) {
    DCHECK(!scoped_ignore_);
    scoped_ignore_ = std::make_unique<ScopedA11yIgnore>(this);
  }

  // Update the menu item selection.
  if (new_selected_command_id.has_value()) {
    SelectMenuItemWithCommandId(*new_selected_command_id);
  } else {
    views::MenuController::GetActiveInstance()->SelectItemAndOpenSubmenu(
        root_view_);
  }

  auto item_view_to_delete_iter = item_views_by_command_id_.find(command_id);
  DCHECK(item_view_to_delete_iter != item_views_by_command_id_.cend());

  views::View* item_view_to_delete = item_view_to_delete_iter->second;

  // Configure `item_view_to_delete` to serve a11y features.
  views::ViewAccessibility& view_accessibility =
      item_view_to_delete->GetViewAccessibility();

  // Polish the a11y announcement for deletion operation.
  view_accessibility.OverrideDescription(
      l10n_util::GetStringUTF16(IDS_CLIPBOARD_HISTORY_ITEM_DELETION));

  // Enable a11y announcement for the view to be deleted.
  view_accessibility.OverrideIsIgnored(false);

  // Disabling `item_view_to_delete` is more like implementation details.
  // So do not expose it to users.
  view_accessibility.OverrideIsEnabled(true);

  // Specify `item_view_to_delete`'s position in the set. Without calling
  // `OverridePosInSet()`, the menu's size after deletion may be announced.
  const int pos_in_set = std::distance(item_views_by_command_id_.begin(),
                                       item_view_to_delete_iter) +
                         1;
  view_accessibility.OverridePosInSet(pos_in_set,
                                      item_views_by_command_id_.size());

  // Disable views to be removed in order to prevent them from handling
  // events.
  root_view_->GetMenuItemByID(command_id)->SetEnabled(false);
  item_view_to_delete->SetEnabled(false);

  item_views_by_command_id_.erase(item_view_to_delete_iter);

  auto item_to_delete = item_snapshots_.find(command_id);
  DCHECK(item_to_delete != item_snapshots_.end());
  item_snapshots_.erase(item_to_delete);

  // The current selected menu item may be accessed after item deletion. So
  // postpone the menu item deletion.
  ++item_deletion_in_progress_count_;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClipboardHistoryMenuModelAdapter::RemoveItemView,
                     weak_ptr_factory_.GetWeakPtr(), command_id));
}

void ClipboardHistoryMenuModelAdapter::AdvancePseudoFocus(bool reverse) {
  absl::optional<int> selected_command = GetSelectedMenuItemCommand();

  // If no item is selected, select the topmost or bottom menu item depending
  // on the focus move direction.
  if (!selected_command.has_value()) {
    SelectMenuItemWithCommandId(
        reverse ? item_views_by_command_id_.rbegin()->first
                : clipboard_history_util::kFirstItemCommandId);
    return;
  }

  AdvancePseudoFocusFromSelectedItem(reverse);
}

clipboard_history_util::Action
ClipboardHistoryMenuModelAdapter::GetActionForCommandId(int command_id) const {
  auto selected_item_iter = item_views_by_command_id_.find(command_id);
  DCHECK(selected_item_iter != item_views_by_command_id_.cend());

  return selected_item_iter->second->action();
}

gfx::Rect ClipboardHistoryMenuModelAdapter::GetMenuBoundsInScreenForTest()
    const {
  DCHECK(root_view_);
  return root_view_->GetSubmenu()->GetBoundsInScreen();
}

const views::MenuItemView*
ClipboardHistoryMenuModelAdapter::GetMenuItemViewAtForTest(size_t index) const {
  DCHECK(root_view_);
  return root_view_->GetSubmenu()->GetMenuItemAt(index);
}

views::MenuItemView* ClipboardHistoryMenuModelAdapter::GetMenuItemViewAtForTest(
    size_t index) {
  return const_cast<views::MenuItemView*>(
      const_cast<const ClipboardHistoryMenuModelAdapter*>(this)
          ->GetMenuItemViewAtForTest(index));
}

ClipboardHistoryMenuModelAdapter::ClipboardHistoryMenuModelAdapter(
    std::unique_ptr<ui::SimpleMenuModel> model,
    base::RepeatingClosure menu_closed_callback,
    const ClipboardHistory* clipboard_history,
    const ClipboardHistoryResourceManager* resource_manager)
    : views::MenuModelAdapter(model.get(), std::move(menu_closed_callback)),
      model_(std::move(model)),
      clipboard_history_(clipboard_history),
      resource_manager_(resource_manager) {}

void ClipboardHistoryMenuModelAdapter::AdvancePseudoFocusFromSelectedItem(
    bool reverse) {
  absl::optional<int> selected_item_command = GetSelectedMenuItemCommand();
  DCHECK(selected_item_command.has_value());
  auto selected_item_iter =
      item_views_by_command_id_.find(*selected_item_command);
  DCHECK(selected_item_iter != item_views_by_command_id_.end());
  ClipboardHistoryItemView* selected_item_view = selected_item_iter->second;

  // Move the pseudo focus on the selected item view. Return early if the
  // focused view does not change.
  const bool selected_item_has_focus =
      selected_item_view->AdvancePseudoFocus(reverse);
  if (selected_item_has_focus)
    return;

  int next_selected_item_command = -1;
  ClipboardHistoryItemView* next_focused_view = nullptr;

  if (reverse) {
    auto next_focused_item_iter =
        selected_item_iter == item_views_by_command_id_.begin()
            ? item_views_by_command_id_.rbegin()
            : std::make_reverse_iterator(selected_item_iter);
    next_selected_item_command = next_focused_item_iter->first;
    next_focused_view = next_focused_item_iter->second;
  } else {
    auto next_focused_item_iter = std::next(selected_item_iter, 1);
    if (next_focused_item_iter == item_views_by_command_id_.end())
      next_focused_item_iter = item_views_by_command_id_.begin();
    next_selected_item_command = next_focused_item_iter->first;
    next_focused_view = next_focused_item_iter->second;
  }

  // Advancing pseudo focus should precede the item selection. Because when an
  // item view is selected, the selected view does not overwrite its pseudo
  // focus if its pseudo focus is non-empty. It can ensure that the pseudo
  // focus and the corresponding UI appearance update only once.
  next_focused_view->AdvancePseudoFocus(reverse);
  SelectMenuItemWithCommandId(next_selected_item_command);
}

int ClipboardHistoryMenuModelAdapter::CalculateSelectedCommandIdAfterDeletion(
    int command_id) const {
  // If the menu item view to be deleted is the last one, Cancel()
  // should be called so this function should not be hit.
  DCHECK_GT(item_snapshots_.size(), 1u);

  auto item_to_delete = item_snapshots_.find(command_id);
  DCHECK(item_to_delete != item_snapshots_.cend());

  // Use the menu item right after the one to be deleted if any. Otherwise,
  // select the previous one.

  auto next_item_iter = item_to_delete;
  ++next_item_iter;
  if (next_item_iter != item_snapshots_.cend())
    return next_item_iter->first;

  auto previous_item_iter = item_to_delete;
  --previous_item_iter;
  return previous_item_iter->first;
}

void ClipboardHistoryMenuModelAdapter::RemoveItemView(int command_id) {
  absl::optional<int> original_selected_command_id =
      GetSelectedMenuItemCommand();

  // The menu item view and its corresponding command should be removed at the
  // same time. Otherwise, it may run into check errors.
  model_->RemoveItemAt(model_->GetIndexOfCommandId(command_id).value());
  root_view_->RemoveMenuItem(root_view_->GetMenuItemByID(command_id));
  root_view_->ChildrenChanged();

  --item_deletion_in_progress_count_;
  // Re-enable a11y for all item views when item deletion finally completes.
  if (!item_deletion_in_progress_count_) {
    DCHECK(scoped_ignore_);
    scoped_ignore_.reset();
  }

  // `ChildrenChanged()` clears the selection. So restore the selection.
  if (original_selected_command_id.has_value())
    SelectMenuItemWithCommandId(*original_selected_command_id);
}

views::MenuItemView* ClipboardHistoryMenuModelAdapter::AppendMenuItem(
    views::MenuItemView* menu,
    ui::MenuModel* model,
    size_t index) {
  const int command_id = model->GetCommandIdAt(index);

  views::MenuItemView* container = menu->AppendMenuItem(command_id);

  // Ignore `container` in accessibility events handling. Let `item_view`
  // handle.
  container->GetViewAccessibility().OverrideIsIgnored(true);

  // Margins are managed by `ClipboardHistoryItemView`.
  container->SetMargins(/*top_margin=*/0, /*bottom_margin=*/0);

  std::unique_ptr<ClipboardHistoryItemView> item_view =
      ClipboardHistoryItemView::CreateFromClipboardHistoryItem(
          GetItemFromCommandId(command_id).id(), clipboard_history_,
          resource_manager_, container);
  item_view->Init();
  item_views_by_command_id_.insert(std::make_pair(command_id, item_view.get()));
  container->AddChildView(std::move(item_view));

  return container;
}

void ClipboardHistoryMenuModelAdapter::OnMenuClosed(views::MenuItemView* menu) {
  // Terminate alive asynchronous calls on `RemoveItemView()`. It is pointless
  // to update views when the menu is closed.
  // Note that data members related to the asynchronous calls, such as
  // `item_deletion_in_progress_count_` and `scoped_ignore_`, are not reset.
  // Because when hitting here, this instance is going to be destructed soon.
  weak_ptr_factory_.InvalidateWeakPtrs();

  ClipboardImageModelFactory::Get()->Deactivate();
  const base::TimeDelta user_journey_time =
      base::TimeTicks::Now() - menu_open_time_;
  UMA_HISTOGRAM_TIMES("Ash.ClipboardHistory.ContextMenu.UserJourneyTime",
                      user_journey_time);
  views::MenuModelAdapter::OnMenuClosed(menu);
  item_views_by_command_id_.clear();

  // This implementation of MenuModelAdapter does not have a widget so we need
  // to manually notify the accessibility side of the closed menu.
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  views::Widget* active_widget =
      views::Widget::GetWidgetForNativeView(active_window);
  DCHECK(active_widget);
  views::View* focused_view =
      active_widget->GetFocusManager()->GetFocusedView();
  if (focused_view) {
    focused_view->NotifyAccessibilityEvent(ax::mojom::Event::kMenuEnd,
                                           /*send_native_event=*/true);
  }
}

}  // namespace ash
