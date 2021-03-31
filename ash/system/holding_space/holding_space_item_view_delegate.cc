// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_view_delegate.h"

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_drag_util.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_tray_bubble.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "net/base/mime_util.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// It is expected that all `HoldingSpaceItemView`s share the same delegate in
// order to support multiple selections. We cache the singleton `instance` in
// order to enforce this requirement.
HoldingSpaceItemViewDelegate* instance = nullptr;

// Helpers ---------------------------------------------------------------------

// Returns the holding space items associated with the specified `views`.
std::vector<const HoldingSpaceItem*> GetItems(
    const std::vector<const HoldingSpaceItemView*>& views) {
  std::vector<const HoldingSpaceItem*> items;
  for (const HoldingSpaceItemView* view : views)
    items.push_back(view->item());
  return items;
}

// Returns the subset of `views` in the range of `start` and `end` (inclusive).
std::vector<HoldingSpaceItemView*> GetViewsInRange(
    const std::vector<HoldingSpaceItemView*>& views,
    HoldingSpaceItemView* start,
    HoldingSpaceItemView* end) {
  if (!start || !end)
    return {};

  bool found_start = false;
  bool found_end = false;

  std::vector<HoldingSpaceItemView*> range;
  for (HoldingSpaceItemView* view : views) {
    if (view == start)
      found_start = true;
    if (view == end)
      found_end = true;
    if (found_start || found_end)
      range.push_back(view);
    if (found_start && found_end)
      break;
  }

  DCHECK(found_start);
  DCHECK(found_end);
  return range;
}

// Attempts to open the holding space items associated with the given `views`.
void OpenItems(const std::vector<const HoldingSpaceItemView*>& views) {
  DCHECK_GE(views.size(), 1u);
  HoldingSpaceController::Get()->client()->OpenItems(GetItems(views),
                                                     base::DoNothing());
}

}  // namespace

// HoldingSpaceItemViewDelegate::ScopedSelectionRestore ------------------------

HoldingSpaceItemViewDelegate::ScopedSelectionRestore::ScopedSelectionRestore(
    HoldingSpaceItemViewDelegate* delegate)
    : delegate_(delegate) {
  // Save selection.
  for (const HoldingSpaceItemView* view : delegate_->GetSelection())
    selected_item_ids_.push_back(view->item_id());

  // Save `selected_range_start_`.
  if (delegate_->selected_range_start_)
    selected_range_start_item_id_ = delegate_->selected_range_start_->item_id();

  // Save `selected_range_end_`.
  if (delegate_->selected_range_end_)
    selected_range_end_item_id_ = delegate_->selected_range_end_->item_id();
}

HoldingSpaceItemViewDelegate::ScopedSelectionRestore::
    ~ScopedSelectionRestore() {
  // Restore selection.
  delegate_->SetSelection(selected_item_ids_);

  if (!selected_range_start_item_id_ || !selected_range_end_item_id_)
    return;

  HoldingSpaceItemView* selected_range_start = nullptr;
  HoldingSpaceItemView* selected_range_end = nullptr;

  for (HoldingSpaceItemView* view :
       delegate_->bubble_->GetHoldingSpaceItemViews()) {
    // Cache `selected_range_start`.
    if (selected_range_start_item_id_ == view->item_id())
      selected_range_start = view;

    // Cache `selected_range_end`.
    if (selected_range_end_item_id_ == view->item_id())
      selected_range_end = view;

    // Restore `selected_range_start_` and `selected_range_end_` iff both are
    // still in existence during the restoration process.
    if (selected_range_start && selected_range_end) {
      delegate_->selected_range_start_ = selected_range_start;
      delegate_->selected_range_end_ = selected_range_end;
      break;
    }
  }
}

// HoldingSpaceItemViewDelegate ------------------------------------------------

HoldingSpaceItemViewDelegate::HoldingSpaceItemViewDelegate(
    HoldingSpaceTrayBubble* bubble)
    : bubble_(bubble) {
  DCHECK_EQ(nullptr, instance);
  instance = this;

  // Multi-select is the only selection UI in tablet mode. Outside of tablet
  // mode, selection UI is based on the `selection_size_`.
  selection_ui_ = TabletMode::Get()->InTabletMode()
                      ? SelectionUi::kMultiSelect
                      : SelectionUi::kSingleSelect;

  tablet_mode_observer_.Observe(TabletMode::Get());
}

HoldingSpaceItemViewDelegate::~HoldingSpaceItemViewDelegate() {
  DCHECK_EQ(instance, this);
  instance = nullptr;
}

void HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewCreated(
    HoldingSpaceItemView* view) {
  if (view->selected()) {
    ++selection_size_;
    UpdateSelectionUi();
  }
}

void HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewDestroying(
    HoldingSpaceItemView* view) {
  // If either endpoint of the selected range is destroyed, clear the cache so
  // that the next range-based selection attempt will start from scratch.
  if (selected_range_start_ == view || selected_range_end_ == view) {
    selected_range_start_ = nullptr;
    selected_range_end_ = nullptr;
  }

  if (view->selected()) {
    --selection_size_;
    UpdateSelectionUi();
  }
}

bool HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewAccessibleAction(
    HoldingSpaceItemView* view,
    const ui::AXActionData& action_data) {
  // When performing the default accessible action (e.g. Search + Space), open
  // the selected holding space items. If `view` is not part of the current
  // selection it will become the entire selection.
  if (action_data.action == ax::mojom::Action::kDoDefault) {
    if (!view->selected())
      SetSelection(view);
    OpenItems(GetSelection());
    return true;
  }
  // When showing the context menu via accessible action (e.g. Search + M),
  // ensure that `view` is part of the current selection. If it is not part of
  // the current selection it will become the entire selection.
  if (action_data.action == ax::mojom::Action::kShowContextMenu) {
    if (!view->selected())
      SetSelection(view);
    // Return false so that the views framework will show the context menu.
    return false;
  }
  return false;
}

bool HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewGestureEvent(
    HoldingSpaceItemView* view,
    const ui::GestureEvent& event) {
  // The user may alternate between using mouse and touch inputs. Treat gesture
  // events as mouse events when tracking range-based selection so that if
  // the user switches back to using mouse input, selection state will be
  // determined based on this most recent interaction with `view`.
  selected_range_start_ = view;
  selected_range_end_ = view;

  // When a long press or two finger tap gesture occurs we are going to show the
  // context menu. Ensure that the pressed `view` is part of the selection.
  if (event.type() == ui::ET_GESTURE_LONG_PRESS ||
      event.type() == ui::ET_GESTURE_TWO_FINGER_TAP) {
    view->SetSelected(true);
    return false;
  }
  // If a scroll begin gesture is received while the context menu is showing,
  // that means the user is trying to initiate a drag. Close the context menu
  // and start the item drag.
  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN && context_menu_runner_ &&
      context_menu_runner_->IsRunning()) {
    context_menu_runner_.reset();
    view->StartDrag(event, ui::mojom::DragEventSource::kTouch);
    return false;
  }

  if (event.type() != ui::ET_GESTURE_TAP)
    return false;

  // When a tap gesture occurs and *no* views are currently selected, select and
  // open the tapped `view`. Note that the tap `event` should *not* propagate
  // further. Failure to halt propagation would result in the gesture reaching
  // the child bubble which clears selection state.
  if (GetSelection().empty()) {
    SetSelection(view);
    OpenItems(GetSelection());
    return true;
  }

  // When a tap gesture occurs and a selection *does* exist, the selected state
  // of the tapped `view` is toggled. Note that the tap event should *not*
  // propagate further. Failure to halt propagation would result in the gesture
  // reaching the child bubble which clears selection state.
  view->SetSelected(!view->selected());
  return true;
}

bool HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewKeyPressed(
    HoldingSpaceItemView* view,
    const ui::KeyEvent& event) {
  // The ENTER key should open all selected holding space items. If `view` isn't
  // already part of the selection, it will become the entire selection.
  if (event.key_code() == ui::KeyboardCode::VKEY_RETURN) {
    if (!view->selected())
      SetSelection(view);
    OpenItems(GetSelection());
    return true;
  }
  return false;
}

bool HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewMousePressed(
    HoldingSpaceItemView* view,
    const ui::MouseEvent& event) {
  // Since we are starting a new mouse pressed/released sequence, we need to
  // clear any view that we had cached to ignore mouse released events for.
  ignore_mouse_released_ = nullptr;

  // If SHIFT is *not* pressed, set `view` as the starting point for range-based
  // selection so that the next time the user shift-clicks, selection state
  // will be updated in the range of `view` and the view being shift-clicked.
  // Note that `view` is also set as the starting point if previously unset.
  if (!event.IsShiftDown() || !selected_range_start_)
    selected_range_start_ = view;

  // When a `view` is pressed it becomes the new end for range-based selection.
  // Note that this is performed in a scoped closure runner in order to give
  // `SetSelectedRange()` a chance to run and clean up any previous range-based
  // selection.
  base::ScopedClosureRunner set_selected_range_end(base::BindOnce(
      [](HoldingSpaceItemView** selected_range_end,
         HoldingSpaceItemView* view) { *selected_range_end = view; },
      &selected_range_end_, view));

  // If the SHIFT key is down, the user is attempting a range-based selection.
  // Remove from the selection the previously selected range and instead add
  // the newly selected range to the selection. Note that the next mouse
  // released event on `view` is ignored if this is not a double click so that
  // `view` isn't accidentally unselected right after having selected it. If
  // this is a double click, the mouse released event will be handled to launch
  // the selected holding space items.
  if (event.IsShiftDown()) {
    if (!(event.flags() & ui::EF_IS_DOUBLE_CLICK))
      ignore_mouse_released_ = view;
    SetSelectedRange(selected_range_start_, /*end=*/view);
    return true;
  }

  // If the `view` is already selected, mouse press is a no-op. Actions taken on
  // selected views are performed on mouse released in order to give drag/drop
  // a chance to take effect (assuming that drag thresholds are met).
  if (view->selected())
    return true;

  // If the CTRL key is down, we need to add `view` to the current selection.
  // We're going to need to ignore the next mouse released event on `view` if
  // this is not a double click so that we don't unselect `view` accidentally
  // right after having selected it. If this is a double click, the mouse
  // released event will be handled to launch the selected holding space items.
  if (event.IsControlDown()) {
    if (!(event.flags() & ui::EF_IS_DOUBLE_CLICK))
      ignore_mouse_released_ = view;
    view->SetSelected(true);
    return true;
  }

  // In the absence of any modifiers, pressing an unselected `view` will cause
  // `view` to become the current selection. Previous selections are cleared.
  SetSelection(view);
  return true;
}

void HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewMouseReleased(
    HoldingSpaceItemView* view,
    const ui::MouseEvent& event) {
  // We should always clear `ignore_mouse_released_` since that property should
  // affect at most one press/release sequence.
  views::View* const old_ignore_mouse_released = ignore_mouse_released_;
  ignore_mouse_released_ = nullptr;

  // We might be ignoring mouse released events for `view` if it was just
  // selected on mouse pressed. In this case, no-op here.
  if (old_ignore_mouse_released == view)
    return;

  // If the right mouse button is released we're showing the context menu. In
  // this case, no-op here.
  if (event.IsRightMouseButton())
    return;

  // If this mouse released `event` is part of a double click, we should open
  // the items associated with the current selection. It is expected that the
  // `view` being clicked is already part of the selection.
  if (event.flags() & ui::EF_IS_DOUBLE_CLICK) {
    DCHECK(view->selected());
    OpenItems(GetSelection());
    return;
  }

  // If the CTRL key is down, mouse release should toggle the selected state of
  // `view`. It's possible that the current selection be empty after doing so.
  if (event.IsControlDown()) {
    view->SetSelected(!view->selected());
    return;
  }

  // This mouse released `event` is not part of a double click, nor were there
  // any modifiers which resulted in special handling. In this case, the `view`
  // under the mouse should become the only selected view.
  SetSelection(view);
}

void HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewSelectedChanged(
    HoldingSpaceItemView* view) {
  selection_size_ += view->selected() ? 1 : -1;
  UpdateSelectionUi();
}

bool HoldingSpaceItemViewDelegate::OnHoldingSpaceTrayBubbleKeyPressed(
    const ui::KeyEvent& event) {
  // The ENTER key should open all selected holding space items.
  if (event.key_code() == ui::KeyboardCode::VKEY_RETURN) {
    if (!GetSelection().empty()) {
      OpenItems(GetSelection());
      return true;
    }
  }
  return false;
}

void HoldingSpaceItemViewDelegate::OnHoldingSpaceTrayChildBubbleGestureEvent(
    const ui::GestureEvent& event) {
  if (event.type() == ui::ET_GESTURE_TAP)
    ClearSelection();
}

void HoldingSpaceItemViewDelegate::OnHoldingSpaceTrayChildBubbleMousePressed(
    const ui::MouseEvent& event) {
  ClearSelection();
}

base::RepeatingClosureList::Subscription
HoldingSpaceItemViewDelegate::AddSelectionUiChangedCallback(
    base::RepeatingClosureList::CallbackType callback) {
  return selection_ui_changed_callbacks_.Add(std::move(callback));
}

void HoldingSpaceItemViewDelegate::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // In touch mode, gesture events continue to be sent to holding space views
  // after showing the context menu so that it can be aborted if the user
  // initiates a drag sequence. This means both `ui::ET_GESTURE_LONG_TAP` and
  // `ui::ET_GESTURE_LONG_PRESS` may be received while showing the context menu
  // which would result in trying to show the context menu twice. This would not
  // be a fatal failure but would result in UI jank.
  if (context_menu_runner_ && context_menu_runner_->IsRunning())
    return;

  int run_types = views::MenuRunner::USE_TOUCHABLE_LAYOUT |
                  views::MenuRunner::CONTEXT_MENU |
                  views::MenuRunner::FIXED_ANCHOR;

  // In touch mode the context menu may be aborted if the user initiates a drag.
  // In order to determine if the gesture resulting in this context menu being
  // shown was actually the start of a drag sequence, holding space views will
  // have to receive events that would otherwise be consumed by the `MenuHost`.
  if (source_type == ui::MenuSourceType::MENU_SOURCE_TOUCH)
    run_types |= views::MenuRunner::SEND_GESTURE_EVENTS_TO_OWNER;

  context_menu_runner_ =
      std::make_unique<views::MenuRunner>(BuildMenuModel(), run_types);

  context_menu_runner_->RunMenuAt(
      source->GetWidget(), nullptr /*button_controller*/,
      source->GetBoundsInScreen(), views::MenuAnchorPosition::kBubbleBelow,
      source_type);
}

bool HoldingSpaceItemViewDelegate::CanStartDragForView(
    views::View* sender,
    const gfx::Point& press_pt,
    const gfx::Point& current_pt) {
  const gfx::Vector2d delta = current_pt - press_pt;
  return views::View::ExceededDragThreshold(delta);
}

int HoldingSpaceItemViewDelegate::GetDragOperationsForView(
    views::View* sender,
    const gfx::Point& press_pt) {
  return ui::DragDropTypes::DRAG_COPY;
}

void HoldingSpaceItemViewDelegate::WriteDragDataForView(
    views::View* sender,
    const gfx::Point& press_pt,
    ui::OSExchangeData* data) {
  std::vector<const HoldingSpaceItemView*> selection = GetSelection();
  DCHECK_GE(selection.size(), 1u);

  holding_space_metrics::RecordItemAction(
      GetItems(selection), holding_space_metrics::ItemAction::kDrag);

  // Drag image.
  gfx::ImageSkia drag_image;
  gfx::Vector2d drag_offset;
  holding_space_util::CreateDragImage(selection, &drag_image, &drag_offset);
  data->provider().SetDragImage(std::move(drag_image), drag_offset);

  // Payload.
  std::vector<ui::FileInfo> filenames;
  for (const HoldingSpaceItemView* view : selection) {
    const base::FilePath& file_path = view->item()->file_path();
    filenames.push_back(ui::FileInfo(file_path, file_path.BaseName()));
  }
  data->SetFilenames(filenames);
}

void HoldingSpaceItemViewDelegate::ExecuteCommand(int command_id,
                                                  int event_flags) {
  std::vector<const HoldingSpaceItemView*> selection = GetSelection();
  DCHECK_GE(selection.size(), 1u);

  switch (static_cast<HoldingSpaceCommandId>(command_id)) {
    case HoldingSpaceCommandId::kCopyImageToClipboard:
      DCHECK_EQ(selection.size(), 1u);
      HoldingSpaceController::Get()->client()->CopyImageToClipboard(
          *selection.front()->item(), base::DoNothing());
      break;
    case HoldingSpaceCommandId::kRemoveItem:
      HoldingSpaceController::Get()->model()->RemoveIf(base::BindRepeating(
          [](const std::vector<const HoldingSpaceItemView*>& selection,
             const HoldingSpaceItem* item) {
            const bool remove =
                std::any_of(selection.begin(), selection.end(),
                            [item](const HoldingSpaceItemView* view) {
                              return view->item() == item;
                            });
            if (remove) {
              holding_space_metrics::RecordItemAction(
                  {item}, holding_space_metrics::ItemAction::kRemove);
            }
            return remove;
          },
          std::cref(selection)));
      break;
    case HoldingSpaceCommandId::kPinItem:
      HoldingSpaceController::Get()->client()->PinItems(GetItems(selection));
      break;
    case HoldingSpaceCommandId::kShowInFolder:
      DCHECK_EQ(selection.size(), 1u);
      HoldingSpaceController::Get()->client()->ShowItemInFolder(
          *selection.front()->item(), base::DoNothing());
      break;
    case HoldingSpaceCommandId::kUnpinItem:
      HoldingSpaceController::Get()->client()->UnpinItems(GetItems(selection));
      break;
    default:
      NOTREACHED();
      break;
  }
}

void HoldingSpaceItemViewDelegate::OnTabletModeStarted() {
  UpdateSelectionUi();
}

void HoldingSpaceItemViewDelegate::OnTabletModeEnded() {
  UpdateSelectionUi();
}

ui::SimpleMenuModel* HoldingSpaceItemViewDelegate::BuildMenuModel() {
  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  std::vector<const HoldingSpaceItemView*> selection = GetSelection();
  DCHECK_GE(selection.size(), 1u);

  if (selection.size() == 1u) {
    // The "Show in folder" command should only be present if there is only one
    // holding space item selected.
    context_menu_model_->AddItemWithIcon(
        static_cast<int>(HoldingSpaceCommandId::kShowInFolder),
        l10n_util::GetStringUTF16(
            IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_SHOW_IN_FOLDER),
        ui::ImageModel::FromVectorIcon(kFolderIcon, /*color_id=*/-1,
                                       kHoldingSpaceIconSize));

    std::string mime_type;
    const bool is_image =
        net::GetMimeTypeFromFile(selection.front()->item()->file_path(),
                                 &mime_type) &&
        net::MatchesMimeType(kMimeTypeImage, mime_type);

    if (is_image) {
      // The "Copy image" command should only be present if there is only one
      // holding space item selected and that item is backed by an image file.
      context_menu_model_->AddItemWithIcon(
          static_cast<int>(HoldingSpaceCommandId::kCopyImageToClipboard),
          l10n_util::GetStringUTF16(
              IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_COPY_IMAGE_TO_CLIPBOARD),
          ui::ImageModel::FromVectorIcon(kCopyIcon, /*color_id=*/-1,
                                         kHoldingSpaceIconSize));
    }
  }

  const bool is_any_unpinned = std::any_of(
      selection.begin(), selection.end(), [](const HoldingSpaceItemView* view) {
        return !HoldingSpaceController::Get()->model()->ContainsItem(
            HoldingSpaceItem::Type::kPinnedFile, view->item()->file_path());
      });

  if (is_any_unpinned) {
    // The "Pin" command should be present if any selected holding space item is
    // unpinned. When executing this command, any holding space items that are
    // already pinned will be ignored.
    context_menu_model_->AddItemWithIcon(
        static_cast<int>(HoldingSpaceCommandId::kPinItem),
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_PIN),
        ui::ImageModel::FromVectorIcon(views::kPinIcon, /*color_id=*/-1,
                                       kHoldingSpaceIconSize));
  } else {
    // The "Unpin" command should be present only if all selected holding space
    // items are already pinned.
    context_menu_model_->AddItemWithIcon(
        static_cast<int>(HoldingSpaceCommandId::kUnpinItem),
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_UNPIN),
        ui::ImageModel::FromVectorIcon(views::kUnpinIcon, /*color_id=*/-1,
                                       kHoldingSpaceIconSize));
  }

  const bool is_removable = std::all_of(
      selection.begin(), selection.end(), [](const HoldingSpaceItemView* view) {
        return view->item()->type() != HoldingSpaceItem::Type::kPinnedFile;
      });

  if (is_removable) {
    context_menu_model_->AddItemWithIcon(
        static_cast<int>(HoldingSpaceCommandId::kRemoveItem),
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_REMOVE),
        ui::ImageModel::FromVectorIcon(kRemoveCircleOutlineIcon,
                                       /*color_id=*/-1, kHoldingSpaceIconSize));
  }

  return context_menu_model_.get();
}

std::vector<const HoldingSpaceItemView*>
HoldingSpaceItemViewDelegate::GetSelection() {
  std::vector<const HoldingSpaceItemView*> selection;
  for (const HoldingSpaceItemView* view : bubble_->GetHoldingSpaceItemViews()) {
    if (view->selected())
      selection.push_back(view);
  }
  DCHECK_EQ(selection.size(), selection_size_);
  return selection;
}

void HoldingSpaceItemViewDelegate::ClearSelection() {
  SetSelection(std::vector<std::string>());
}

void HoldingSpaceItemViewDelegate::SetSelection(
    HoldingSpaceItemView* selection) {
  SetSelection({selection->item_id()});
}

void HoldingSpaceItemViewDelegate::SetSelection(
    const std::vector<std::string>& item_ids) {
  std::vector<HoldingSpaceItemView*> selection;

  for (HoldingSpaceItemView* view : bubble_->GetHoldingSpaceItemViews()) {
    view->SetSelected(base::Contains(item_ids, view->item_id()));
    if (view->selected())
      selection.push_back(view);
  }

  if (selection.size() == 1u) {
    selected_range_start_ = selection.front();
    selected_range_end_ = selection.front();
  } else {
    selected_range_start_ = nullptr;
    selected_range_end_ = nullptr;
  }
}

void HoldingSpaceItemViewDelegate::SetSelectedRange(HoldingSpaceItemView* start,
                                                    HoldingSpaceItemView* end) {
  const std::vector<HoldingSpaceItemView*> views =
      bubble_->GetHoldingSpaceItemViews();

  for (HoldingSpaceItemView* view :
       GetViewsInRange(views, selected_range_start_, selected_range_end_)) {
    view->SetSelected(false);
  }

  selected_range_start_ = start;
  selected_range_end_ = end;

  for (HoldingSpaceItemView* view :
       GetViewsInRange(views, selected_range_start_, selected_range_end_)) {
    view->SetSelected(true);
  }
}

void HoldingSpaceItemViewDelegate::UpdateSelectionUi() {
  const SelectionUi selection_ui =
      TabletMode::Get()->InTabletMode() || selection_size_ > 1u
          ? SelectionUi::kMultiSelect
          : SelectionUi::kSingleSelect;

  if (selection_ui_ == selection_ui)
    return;

  selection_ui_ = selection_ui;
  selection_ui_changed_callbacks_.Notify();
}

}  // namespace ash
