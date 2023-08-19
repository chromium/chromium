// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/button_label_list.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/radio_button_group.h"
#include "ash/style/rounded_container.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_class_properties.h"

namespace arc::input_overlay {

namespace {

constexpr int kMenuWidth = 316;

}  // namespace

ButtonLabelList::ButtonLabelList(
    DisplayOverlayController* display_overlay_controller,
    Action* action)
    : display_overlay_controller_(display_overlay_controller), action_(action) {
  Init();
}

ButtonLabelList::~ButtonLabelList() = default;

void ButtonLabelList::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddHeader();
  AddActionLabels();
}

void ButtonLabelList::AddHeader() {
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
      base::BindRepeating(&ButtonLabelList::OnBackButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kBackArrowTouchIcon,
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));

  container->AddChildView(ash::bubble_utils::CreateLabel(
      // TODO(b/274690042): Replace placeholder text with localized strings.
      ash::TypographyToken::kCrosTitle1, u"Action list",
      cros_tokens::kCrosSysOnSurface));
}

void ButtonLabelList::AddActionLabels() {
  // "container" uses the default background color of "ash::RoundedContainer".
  auto* container = AddChildView(std::make_unique<ash::RoundedContainer>());
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  button_group_ =
      container->AddChildView(std::make_unique<ash::RadioButtonGroup>(
          /*group_width=*/kMenuWidth - 32,
          /*insider_border_insets=*/gfx::Insets::VH(8, 8),
          /*between_child_spacing=*/0,
          /*icon_direction=*/ash::RadioButton::IconDirection::kFollowing,
          /*icon_type=*/ash::RadioButton::IconType::kCheck,
          /*radio_button_padding=*/gfx::Insets::VH(10, 10),
          /*radio_button_image_label_padding=*/
          ash::RadioButton::kImageLabelSpacingDP));

  const auto& action_name_list =
      display_overlay_controller_->action_name_list();
  for (size_t index = 0; index < action_name_list.size(); index++) {
    const auto& action_name = action_name_list[index];
    auto* button = button_group_->AddButton(
        base::BindRepeating(&ButtonLabelList::OnActionLabelPressed,
                            base::Unretained(this)),
        action_name);

    if (action_->name_label_index() == static_cast<int>(index)) {
      button->SetSelected(true);
    }
  }
}

void ButtonLabelList::OnActionLabelPressed() {
  auto* selected_button = button_group_->GetSelectedButtons()[0];
  auto action_name = selected_button->GetText();
  int index = GetIndexOfActionName(
      display_overlay_controller_->action_name_list(), action_name);
  DCHECK(index >= 0);
  display_overlay_controller_->ChangeActionName(action_, index);
  OnBackButtonPressed();
}

void ButtonLabelList::OnBackButtonPressed() {
  display_overlay_controller_->OnButtonLabelListBackButtonPressed();
}

}  // namespace arc::input_overlay
