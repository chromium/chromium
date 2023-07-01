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
#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_class_properties.h"

namespace arc::input_overlay {

namespace {

constexpr int kMenuWidth = 316;
constexpr int kTriangleHeight = 14;
constexpr int kMenuActionSpacing = 8;

}  // namespace

// static
ButtonLabelList* ButtonLabelList::Show(DisplayOverlayController* controller,
                                       ButtonOptionsMenu* button_options_menu) {
  auto* parent = controller->GetOverlayWidgetContentsView();
  auto* action_list = parent->AddChildView(
      std::make_unique<ButtonLabelList>(controller, button_options_menu));
  action_list->Init();
  return action_list;
}

ButtonLabelList::ButtonLabelList(
    DisplayOverlayController* display_overlay_controller,
    ButtonOptionsMenu* button_options_menu)
    : display_overlay_controller_(display_overlay_controller),
      button_options_menu_(button_options_menu) {}

ButtonLabelList::~ButtonLabelList() = default;

void ButtonLabelList::Init() {
  SetUseDefaultFillLayout(true);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetBorder(views::CreateEmptyBorder(
      button_options_menu_->action()->on_left_or_middle_side()
          ? gfx::Insets::TLBR(16, 16 + kTriangleHeight, 16, 16)
          : gfx::Insets::TLBR(16, 16, 16, 16 + kTriangleHeight)));

  AddHeader();
  AddActionLabels();

  SizeToPreferredSize();
  CalculatePosition();
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
          /*radio_button_padding=*/gfx::Insets::VH(10, 10)));

  auto name_label = button_options_menu_->action()->name_label();
  // TODO(b/274690042): Replace placeholder text with localized strings.
  const std::vector<std::u16string> action_name_list = {
      u"Move", u"Jump",  u"Attack", u"Special ability", u"Crouch",
      u"Run",  u"Shoot", u"Magic",  u"Reload",          u"Dodge"};
  for (const auto& action_name : action_name_list) {
    auto* button = button_group_->AddButton(
        base::BindRepeating(&ButtonLabelList::OnActionLabelPressed,
                            base::Unretained(this)),
        action_name);
    if (name_label && !(*name_label).compare(action_name)) {
      button->SetSelected(true);
    }
  }
}

void ButtonLabelList::OnActionLabelPressed() {
  auto* selected_button = button_group_->GetSelectedButtons()[0];
  display_overlay_controller_->ChangeActionName(button_options_menu_->action(),
                                                selected_button->GetText());
}

void ButtonLabelList::OnBackButtonPressed() {
  button_options_menu_->SetVisible(true);
  display_overlay_controller_->RemoveButtonLabelList();
}

void ButtonLabelList::OnPaintBackground(gfx::Canvas* canvas) {
  int height = GetHeightForWidth(kMenuWidth);
  ui::ColorProvider* color_provider = GetColorProvider();
  DrawBackgroundContainerWithArrow(
      canvas, height, button_options_menu_->action()->on_left_or_middle_side(),
      // TODO(b/289536822): Modify offset calculation.
      button_options_menu_->CalculateActionOffset(height),
      color_provider->GetColor(cros_tokens::kCrosSysBaseElevated),
      color_provider->GetColor(cros_tokens::kCrosSysSystemBorder1));
}

gfx::Size ButtonLabelList::CalculatePreferredSize() const {
  return gfx::Size(kMenuWidth, GetHeightForWidth(kMenuWidth));
}

void ButtonLabelList::CalculatePosition() {
  auto* action = button_options_menu_->action();
  auto* action_view = action->action_view();
  int x = action_view->x();
  int y = action->GetUICenterPosition().y();
  auto parent_size =
      display_overlay_controller_->GetOverlayWidgetContentsView()->size();

  if (action->on_left_or_middle_side()) {
    x += action_view->width() + kMenuActionSpacing;
  } else {
    x -= width() + kMenuActionSpacing;
  }

  y -= height() / 2;
  // The range of values for the y-position of the menu are [0, parent_height -
  // height]. If the calculated y-position is beyond this range, adjust it based
  // on whether the y-position is over or under the range by setting it to the
  // maximum or minimum value respectively.
  if (y > parent_size.height() - height()) {
    y = std::max(0, parent_size.height() - height());
  } else if (y < 0) {
    y = 0;
  }

  SetPosition(gfx::Point(x, y));
}

}  // namespace arc::input_overlay
