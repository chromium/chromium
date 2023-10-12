// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/delete_edit_shortcut.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace arc::input_overlay {

namespace {

constexpr int kSpaceFromEditingList = 8;

}  // namespace

DeleteEditShortcut::DeleteEditShortcut(DisplayOverlayController* controller,
                                       ActionViewListItem* anchor_view)
    : controller_(controller), anchor_view_(anchor_view) {
  Init();
  observation_.Observe(anchor_view);
}

DeleteEditShortcut::~DeleteEditShortcut() {
  observation_.Reset();
}

void DeleteEditShortcut::UpdateAnchorView(ActionViewListItem* anchor_view) {
  observation_.Reset();
  observation_.Observe(anchor_view);
  anchor_view_ = anchor_view;
  UpdateWidget();
}

void DeleteEditShortcut::VisibilityChanged(views::View* starting_from,
                                           bool is_visible) {
  if (is_visible) {
    UpdateWidget();
  }
}

void DeleteEditShortcut::OnMouseExited(const ui::MouseEvent& event) {
  views::View::OnMouseExited(event);
  if (!IsMouseHovered()) {
    controller_->RemoveDeleteEditShortcutWidget();
  }
}

void DeleteEditShortcut::OnViewRemovedFromWidget(views::View*) {
  controller_->RemoveDeleteEditShortcutWidget();
}

void DeleteEditShortcut::OnViewBoundsChanged(views::View*) {
  controller_->RemoveDeleteEditShortcutWidget();
}

void DeleteEditShortcut::Init() {
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(12, 12)));
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevatedOpaque,
      /*radius=*/20,
      /*for_border_thickness=*/0));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /*inside_border_insets=*/gfx::Insets(),
      /*between_child_spacing=*/12));

  AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&DeleteEditShortcut::OnEditButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kGameControlsEditPenIcon,
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));

  AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&DeleteEditShortcut::OnDeleteButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kGameControlsDeleteIcon,
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
}

void DeleteEditShortcut::OnEditButtonPressed() {
  controller_->AddButtonOptionsMenuWidget(anchor_view_->action());
  controller_->RemoveDeleteEditShortcutWidget();
}

void DeleteEditShortcut::OnDeleteButtonPressed() {
  controller_->RemoveAction(anchor_view_->action());
}

void DeleteEditShortcut::UpdateWidget() {
  auto* widget = GetWidget();
  DCHECK(widget);

  auto anchor_point = anchor_view_->GetBoundsInScreen().top_right();
  auto preferred_size = GetPreferredSize();
  anchor_point.Offset(kEditingListInsideBorderInsets + kSpaceFromEditingList,
                      (anchor_view_->height() - preferred_size.height()) / 2);
  widget->SetBounds(gfx::Rect(anchor_point, preferred_size));
  widget->StackAtTop();
}

BEGIN_METADATA(DeleteEditShortcut, views::View)
END_METADATA

}  // namespace arc::input_overlay
