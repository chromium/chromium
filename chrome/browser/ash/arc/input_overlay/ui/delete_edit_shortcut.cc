// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/delete_edit_shortcut.h"

#include <memory>

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
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace arc::input_overlay {

namespace {

constexpr int kSpaceToEditingList = 8;
constexpr char kDeleteEditShortcut[] = "DeleteEditShortcut";

}  // namespace

DeleteEditShortcut::DeleteEditShortcut(DisplayOverlayController* controller,
                                       ActionViewListItem* anchor_view)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::LEFT_CENTER,
                                      views::BubbleBorder::NO_SHADOW),
      controller_(controller) {
  set_margins(gfx::Insets(12));
  set_corner_radius(20);
  set_close_on_deactivate(false);
  set_focus_traversable_from_anchor_view(true);
  set_internal_name(kDeleteEditShortcut);
  set_parent_window(anchor_view->GetWidget()->GetNativeWindow());
  SetButtons(ui::DIALOG_BUTTON_NONE);

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

DeleteEditShortcut::~DeleteEditShortcut() = default;

void DeleteEditShortcut::UpdateAnchorView(ActionViewListItem* anchor_view) {
  SetAnchorView(anchor_view);
}

void DeleteEditShortcut::OnEditButtonPressed() {
  if (auto* anchor_view =
          views::AsViewClass<ActionViewListItem>(GetAnchorView())) {
    controller_->AddButtonOptionsMenuWidget(anchor_view->action());
  }
}

void DeleteEditShortcut::OnDeleteButtonPressed() {
  if (auto* anchor_view =
          views::AsViewClass<ActionViewListItem>(GetAnchorView())) {
    controller_->RemoveAction(anchor_view->action());
    controller_->RemoveDeleteEditShortcutWidget();
  }
}

std::unique_ptr<views::NonClientFrameView>
DeleteEditShortcut::CreateNonClientFrameView(views::Widget* widget) {
  // Create the customized bubble border.
  auto bubble_border =
      std::make_unique<views::BubbleBorder>(arrow(), GetShadow());
  bubble_border->SetColor(color());
  if (GetParams().round_corners) {
    bubble_border->SetCornerRadius(GetCornerRadius());
  }
  bubble_border->set_avoid_shadow_overlap(true);
  bubble_border->set_insets(
      gfx::Insets::VH(0, kSpaceToEditingList + kEditingListInsideBorderInsets));

  auto frame =
      views::BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(std::move(bubble_border));
  return frame;
}

void DeleteEditShortcut::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();
  if (auto* color_provider = GetColorProvider()) {
    set_color(color_provider->GetColor(
        cros_tokens::kCrosSysSystemBaseElevatedOpaque));
  }
}

void DeleteEditShortcut::OnMouseExited(const ui::MouseEvent& event) {
  if (auto* widget = GetWidget();
      widget->IsMouseEventsEnabled() && !IsMouseHovered()) {
    controller_->RemoveDeleteEditShortcutWidget();
  }
}

BEGIN_METADATA(DeleteEditShortcut, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace arc::input_overlay
