// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/delete_edit_shortcut.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
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
                                      // TODO(b/329895423): Add shadow.
                                      views::BubbleBorder::NO_SHADOW),
      controller_(controller) {
  set_margins(gfx::Insets(12));
  set_corner_radius(20);
  set_close_on_deactivate(false);
  set_focus_traversable_from_anchor_view(true);
  set_internal_name(kDeleteEditShortcut);
  set_parent_window(anchor_view->GetWidget()->GetNativeWindow());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetEnableArrowKeyTraversal(true);

  // BubbleDialogDelegate::GetAccessibleWindowRole() is a final method which
  // can't override. If the window role is `kWindow`, it will force set it to
  // alert dialog and reads all the tooltips inside.
  // SetAccessibleWindowRole(kDialog) can prevent it.
  // SetAccessibleWindowRole(ax::mojom::Role::kMenu) results in screenreader to
  // announce the menu having only one item.
  SetAccessibleWindowRole(ax::mojom::Role::kDialog);

  // Set root view to menu.
  GetViewAccessibility().SetRole(ax::mojom::Role::kMenu);
  SetAccessibleTitle(
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_SHORTCUT_MENU_A11Y_LABEL));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /*inside_border_insets=*/gfx::Insets(),
      /*between_child_spacing=*/12));

  // Create the buttons with empty accessibility names. They will be updated in
  // `UpdateTooltipText`.
  edit_button_ = AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&DeleteEditShortcut::OnEditButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kGameControlsEditPenIcon, u"",
      /*is_togglable=*/false, /*has_border=*/false));
  edit_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);

  delete_button_ = AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&DeleteEditShortcut::OnDeleteButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kGameControlsDeleteIcon, u"",
      /*is_togglable=*/false, /*has_border=*/false));
  delete_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);

  UpdateTooltipText(anchor_view);
}

DeleteEditShortcut::~DeleteEditShortcut() = default;

void DeleteEditShortcut::UpdateAnchorView(ActionViewListItem* anchor_view) {
  // Reset the highlight of previous anchor view.
  SetHighlightedButton(anchor_view);

  SetAnchorView(anchor_view);
  UpdateTooltipText(anchor_view);

  // Clear focus when changing anchor view.
  if (auto* focus_manager = GetFocusManager()) {
    focus_manager->ClearFocus();
  }
}

void DeleteEditShortcut::UpdateTooltipText(ActionViewListItem* anchor_view) {
  const std::u16string action_name = anchor_view->CalculateActionName();
  // e.g. "Edit Button m"
  const std::u16string edit_text = l10n_util::GetStringFUTF16(
      IDS_INPUT_OVERLAY_SHORTCUT_EDIT_A11Y_LABEL_TEMPLATE, action_name);
  edit_button_->SetTooltipText(edit_text);

  // e.g. "Delete Joystick wasd"
  const std::u16string delete_text = l10n_util::GetStringFUTF16(
      IDS_INPUT_OVERLAY_SHORTCUT_DELETE_A11Y_LABEL_TEMPLATE, action_name);
  delete_button_->SetTooltipText(delete_text);
}

void DeleteEditShortcut::OnEditButtonPressed() {
  RecordEditDeleteMenuFunctionTriggered(controller_->GetPackageName(),
                                        EditDeleteMenuFunction::kEdit);
  if (auto* anchor_view =
          views::AsViewClass<ActionViewListItem>(GetAnchorView())) {
    controller_->AddButtonOptionsMenuWidget(anchor_view->action());
  }
}

void DeleteEditShortcut::OnDeleteButtonPressed() {
  RecordEditDeleteMenuFunctionTriggered(controller_->GetPackageName(),
                                        EditDeleteMenuFunction::kDelete);
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
  if (auto* frame_view =
          views::AsViewClass<views::BubbleFrameView>(frame.get())) {
    frame_view->SetBubbleBorder(std::move(bubble_border));
  }
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

BEGIN_METADATA(DeleteEditShortcut)
END_METADATA

}  // namespace arc::input_overlay
