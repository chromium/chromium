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
#include "ash/system/holding_space/holding_space_item_view.h"
#include "base/bind.h"
#include "net/base/mime_util.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/dragdrop/drag_drop_types.h"
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

// Attempts to open the holding space items associated with the given `views`.
void OpenItems(const std::vector<const HoldingSpaceItemView*>& views) {
  DCHECK_GE(views.size(), 1u);
  HoldingSpaceController::Get()->client()->OpenItems(GetItems(views),
                                                     base::DoNothing());
}

}  // namespace

// HoldingSpaceItemViewDelegate ------------------------------------------------

HoldingSpaceItemViewDelegate::HoldingSpaceItemViewDelegate() {
  DCHECK_EQ(nullptr, instance);
  instance = this;
}

HoldingSpaceItemViewDelegate::~HoldingSpaceItemViewDelegate() {
  DCHECK_EQ(instance, this);
  instance = nullptr;
}

void HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewCreated(
    HoldingSpaceItemView* view) {
  view_observer_.Add(view);
  views_.push_back(view);
}

bool HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewAccessibleAction(
    HoldingSpaceItemView* view,
    const ui::AXActionData& action_data) {
  // When performing the default accessible action (e.g. Search + Space), we
  // open the selected holding space items. If `view` is not part of the current
  // selection it will become the entire selection.
  if (action_data.action == ax::mojom::Action::kDoDefault) {
    if (!view->selected())
      SetSelection(view);
    OpenItems(GetSelection());
    return true;
  }
  return false;
}

void HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewGestureEvent(
    HoldingSpaceItemView* view,
    const ui::GestureEvent& event) {
  // When a long press gesture occurs we are going to show the context menu.
  // Ensure that the pressed `view` is the only view selected.
  if (event.type() == ui::ET_GESTURE_LONG_PRESS) {
    SetSelection(view);
    return;
  }
  // When a tap gesture occurs, we select and open only the item corresponding
  // to the tapped `view`.
  if (event.type() == ui::ET_GESTURE_TAP) {
    SetSelection(view);
    OpenItems(GetSelection());
  }
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

  // If the `view` is already selected, mouse press is a no-op. Actions taken on
  // selected views are performed on mouse released in order to give drag/drop
  // a chance to take effect (assuming that drag thresholds are met).
  if (view->selected())
    return true;

  // If the right mouse button is pressed, we're going to be showing the context
  // menu. Make sure that `view` is part of the current selection. If the SHIFT
  // key is not down, it should be the entire selection.
  if (event.IsRightMouseButton()) {
    if (event.IsShiftDown())
      view->SetSelected(true);
    else
      SetSelection(view);
    return true;
  }

  // If the SHIFT key is down, we need to add `view` to the current selection.
  // We're going to need to ignore the next mouse released event on `view` so
  // that we don't unselect `view` accidentally right after having selected it.
  if (event.IsShiftDown()) {
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
  // We should always clear `ignore_mouse_released_` after this method runs
  // since that property should affect at most one press/release sequence.
  base::ScopedClosureRunner clear_ignore_mouse_released(base::BindOnce(
      [](HoldingSpaceItemView** ignore_mouse_released) {
        *ignore_mouse_released = nullptr;
      },
      &ignore_mouse_released_));

  // We might be ignoring mouse released events for `view` if it was just
  // selected on mouse pressed. In this case, no-op here.
  if (ignore_mouse_released_ == view)
    return;

  // If the right mouse button is released we're showing the context menu. In
  // this case, no-op here.
  if (event.IsRightMouseButton())
    return;

  // If the SHIFT key is down, mouse release should toggle the selected state of
  // `view`. If `view` is the only selected view, this is a no-op.
  if (event.IsShiftDown()) {
    if (GetSelection().size() > 1u)
      view->SetSelected(!view->selected());
    return;
  }

  // If this mouse released `event` is part of a double click, we should open
  // the items associated with the current selection.
  if (event.flags() & ui::EF_IS_DOUBLE_CLICK)
    OpenItems(GetSelection());
}

void HoldingSpaceItemViewDelegate::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  const int run_types = views::MenuRunner::USE_TOUCHABLE_LAYOUT |
                        views::MenuRunner::CONTEXT_MENU |
                        views::MenuRunner::FIXED_ANCHOR;

  context_menu_runner_ =
      std::make_unique<views::MenuRunner>(BuildMenuModel(), run_types);

  gfx::Rect bounds = source->GetBoundsInScreen();
  bounds.Inset(gfx::Insets(-kHoldingSpaceContextMenuMargin, 0));

  context_menu_runner_->RunMenuAt(
      source->GetWidget(), nullptr /*button_controller*/, bounds,
      views::MenuAnchorPosition::kTopLeft, source_type);
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

  std::vector<ui::FileInfo> filenames;
  for (const HoldingSpaceItemView* view : selection) {
    filenames.push_back(ui::FileInfo(view->item()->file_path(),
                                     view->item()->file_path().BaseName()));
  }
  data->SetFilenames(filenames);
}

void HoldingSpaceItemViewDelegate::OnViewIsDeleting(views::View* view) {
  base::Erase(views_, view);
  view_observer_.Remove(view);
}

void HoldingSpaceItemViewDelegate::ExecuteCommand(int command_id,
                                                  int event_flags) {
  std::vector<const HoldingSpaceItemView*> selection = GetSelection();
  DCHECK_GE(selection.size(), 1u);

  switch (command_id) {
    case HoldingSpaceCommandId::kCopyImageToClipboard:
      DCHECK_EQ(selection.size(), 1u);
      HoldingSpaceController::Get()->client()->CopyImageToClipboard(
          *selection.front()->item(), base::DoNothing());
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
  }
}

ui::SimpleMenuModel* HoldingSpaceItemViewDelegate::BuildMenuModel() {
  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  std::vector<const HoldingSpaceItemView*> selection = GetSelection();
  DCHECK_GE(selection.size(), 1u);

  if (selection.size() == 1u) {
    // The "Show in folder" command should only be present if there is only one
    // holding space item selected.
    context_menu_model_->AddItemWithIcon(
        HoldingSpaceCommandId::kShowInFolder,
        l10n_util::GetStringUTF16(
            IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_SHOW_IN_FOLDER),
        ui::ImageModel::FromVectorIcon(kFolderIcon));

    std::string mime_type;
    const bool is_image =
        net::GetMimeTypeFromFile(selection.front()->item()->file_path(),
                                 &mime_type) &&
        net::MatchesMimeType(kMimeTypeImage, mime_type);

    if (is_image) {
      // The "Copy image" command should only be present if there is only one
      // holding space item selected and that item is backed by an image file.
      context_menu_model_->AddItemWithIcon(
          HoldingSpaceCommandId::kCopyImageToClipboard,
          l10n_util::GetStringUTF16(
              IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_COPY_IMAGE_TO_CLIPBOARD),
          ui::ImageModel::FromVectorIcon(kCopyIcon));
    }
  }

  const bool is_any_unpinned = std::any_of(
      selection.begin(), selection.end(), [](const HoldingSpaceItemView* view) {
        return !HoldingSpaceController::Get()->model()->GetItem(
            HoldingSpaceItem::GetFileBackedItemId(
                HoldingSpaceItem::Type::kPinnedFile,
                view->item()->file_path()));
      });

  if (is_any_unpinned) {
    // The "Pin" command should be present if any selected holding space item is
    // unpinned. When executing this command, any holding space items that are
    // already pinned will be ignored.
    context_menu_model_->AddItemWithIcon(
        HoldingSpaceCommandId::kPinItem,
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_PIN),
        ui::ImageModel::FromVectorIcon(views::kPinIcon));
  } else {
    // The "Unpin" command should be present only if all selected holding space
    // items are already pinned.
    context_menu_model_->AddItemWithIcon(
        HoldingSpaceCommandId::kUnpinItem,
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_UNPIN),
        ui::ImageModel::FromVectorIcon(views::kUnpinIcon));
  }

  return context_menu_model_.get();
}

std::vector<const HoldingSpaceItemView*>
HoldingSpaceItemViewDelegate::GetSelection() {
  std::vector<const HoldingSpaceItemView*> selection;
  for (const HoldingSpaceItemView* view : views_) {
    if (view->selected())
      selection.push_back(view);
  }
  return selection;
}

void HoldingSpaceItemViewDelegate::SetSelection(views::View* selection) {
  for (HoldingSpaceItemView* view : views_)
    view->SetSelected(view == selection);
}

}  // namespace ash
