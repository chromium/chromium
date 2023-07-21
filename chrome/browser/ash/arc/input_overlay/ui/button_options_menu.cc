// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "ash/system/unified/feature_tile.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_type_button_group.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"

namespace arc::input_overlay {

ButtonOptionsMenu::ButtonOptionsMenu(DisplayOverlayController* controller,
                                     Action* action)
    : TouchInjectorObserver(), controller_(controller), action_(action) {
  controller_->AddTouchInjectorObserver(this);
  Init();
}

ButtonOptionsMenu::~ButtonOptionsMenu() {
  controller_->RemoveTouchInjectorObserver(this);
}

void ButtonOptionsMenu::Init() {
  SetUseDefaultFillLayout(true);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddHeader();
  AddEditTitle();
  AddActionSelection();
  AddActionEdit();
  AddActionNameLabel();
}

void ButtonOptionsMenu::AddHeader() {
  // ------------------------------------
  // ||icon|  |"Button options"|  |icon||
  // ------------------------------------
  auto* container = AddChildView(std::make_unique<views::View>());
  container->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  /*horizontal_resize=*/1.0f,
                  views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/2.0f,
                 views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, views::TableLayout::kFixedSize, 0);
  container->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 16, 0));

  container->AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&ButtonOptionsMenu::OnTrashButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kGameControlsDeleteIcon,
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));

  container->AddChildView(ash::bubble_utils::CreateLabel(
      // TODO(b/274690042): Replace placeholder text with localized strings.
      ash::TypographyToken::kCrosTitle1, u"Button options",
      cros_tokens::kCrosSysOnSurface));

  container->AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&ButtonOptionsMenu::OnDoneButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kGameControlsDoneIcon,
      // TODO(b/279117180): Replace placeholder names with a11y strings.
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
}

void ButtonOptionsMenu::AddEditTitle() {
  // ------------------------------
  // ||"Key assignment"|          |
  // ------------------------------
  auto* container = AddChildView(std::make_unique<views::View>());
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart);
  container->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 12, 0));

  container->AddChildView(ash::bubble_utils::CreateLabel(
      // TODO(b/274690042): Replace placeholder text with localized strings.
      ash::TypographyToken::kCrosBody2, u"Key assignment",
      cros_tokens::kCrosSysOnSurface));
}

void ButtonOptionsMenu::AddActionSelection() {
  // ----------------------------------
  // | |feature_tile| |feature_title| |
  // ----------------------------------
  auto* container = AddChildView(std::make_unique<ash::RoundedContainer>(
      ash::RoundedContainer::Behavior::kTopRounded));
  // Create a 1x2 table with a column padding of 8.
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  container->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 2, 0));

  button_group_ = container->AddChildView(
      ActionTypeButtonGroup::CreateButtonGroup(controller_, action_));
}

void ButtonOptionsMenu::AddActionEdit() {
  // ------------------------------
  // ||"Selected key" |key labels||
  // ||"key"                      |
  // ------------------------------
  action_edit_container_ = AddChildView(std::make_unique<ash::RoundedContainer>(
      ash::RoundedContainer::Behavior::kBottomRounded));
  action_edit_container_
      ->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  /*horizontal_resize=*/1.0f,
                  views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, views::TableLayout::kFixedSize, 0);
  action_edit_container_->SetBorderInsets(gfx::Insets::VH(14, 16));
  action_edit_container_->SetProperty(views::kMarginsKey,
                                      gfx::Insets::TLBR(0, 0, 8, 0));

  // TODO(b/274690042): Replace placeholder text with localized strings.
  key_name_tag_ = action_edit_container_->AddChildView(
      NameTag::CreateNameTag(u"Selected key"));
  labels_view_ =
      action_edit_container_->AddChildView(EditLabels::CreateEditLabels(
          controller_, action_, key_name_tag_, /*set_title=*/false));
}

void ButtonOptionsMenu::AddActionNameLabel() {
  // ------------------------------
  // ||"Button label"           > |
  // ||"Unassigned"               |
  //  -----------------------------
  auto* container = AddChildView(std::make_unique<ash::RoundedContainer>());
  container->SetUseDefaultFillLayout(true);
  container->SetBorderInsets(gfx::Insets::VH(14, 16));

  action_name_tile_ =
      container->AddChildView(std::make_unique<ash::FeatureTile>(
          base::BindRepeating(
              &ButtonOptionsMenu::OnButtonLabelAssignmentPressed,
              base::Unretained(this)),
          /*is_togglable=*/false));
  action_name_tile_->SetID(ash::VIEW_ID_ACCESSIBILITY_FEATURE_TILE);
  action_name_tile_->SetAccessibleName(
      // TODO(b/279117180): Replace placeholder names with a11y strings.
      l10n_util::GetStringUTF16(IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
  // TODO(b/274690042): Replace placeholder text with localized strings.
  action_name_tile_->SetLabel(u"Button label");
  action_name_tile_->SetSubLabel(
      action_->name_label() ? *(action_->name_label()) : u"Unassigned");
  action_name_tile_->SetSubLabelVisibility(true);
  action_name_tile_->CreateDecorativeDrillInArrow();
  action_name_tile_->SetBackground(
      views::CreateSolidBackground(SK_ColorTRANSPARENT));
  action_name_tile_->SetVisible(true);
}

void ButtonOptionsMenu::OnTrashButtonPressed() {
  controller_->RemoveAction(action_);
}

void ButtonOptionsMenu::OnDoneButtonPressed() {
  controller_->SaveToProtoFile();
  controller_->RemoveButtonOptionsMenuWidget();
}

void ButtonOptionsMenu::OnButtonLabelAssignmentPressed() {
  controller_->OnButtonOptionsMenuButtonLabelPressed(action_);
}

void ButtonOptionsMenu::OnActionRemoved(const Action& action) {
  DCHECK_EQ(action_, &action);
  controller_->RemoveButtonOptionsMenuWidget();
}

void ButtonOptionsMenu::OnActionTypeChanged(Action* action,
                                            Action* new_action) {
  DCHECK_EQ(action_, action);
  action_ = new_action;
  button_group_->set_action(new_action);
  action_edit_container_->RemoveChildViewT(labels_view_);
  labels_view_ =
      action_edit_container_->AddChildView(EditLabels::CreateEditLabels(
          controller_, action_, key_name_tag_, /*set_title=*/false));
  controller_->UpdateButtonOptionsMenuWidgetBounds(action_);
}

void ButtonOptionsMenu::OnActionInputBindingUpdated(const Action& action) {
  if (action_ == &action) {
    labels_view_->OnActionInputBindingUpdated();
  }
}

void ButtonOptionsMenu::OnActionNameUpdated(const Action& action) {
  if (action_ == &action) {
    auto name_label = action_->name_label();
    if (name_label) {
      action_name_tile_->SetSubLabel(*name_label);
    }
  }
}

}  // namespace arc::input_overlay
