// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_menu_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/accessibility/floating_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Constants for panel sizing, positioning and coloring.
const int kPanelPositionButtonSize = 36;
const int kPanelPositionButtonPadding = 14;
const int kSeparatorHeight = 16;

}  // namespace

AutoclickMenuView::AutoclickMenuView(AutoclickEventType type,
                                     FloatingMenuPosition position)
    : left_click_button_(
          new FloatingMenuButton(this,
                                 kAutoclickLeftClickIcon,
                                 IDS_ASH_AUTOCLICK_OPTION_LEFT_CLICK,
                                 false /* flip_for_rtl */)),
      right_click_button_(
          new FloatingMenuButton(this,
                                 kAutoclickRightClickIcon,
                                 IDS_ASH_AUTOCLICK_OPTION_RIGHT_CLICK,
                                 false /* flip_for_rtl */)),
      double_click_button_(
          new FloatingMenuButton(this,
                                 kAutoclickDoubleClickIcon,
                                 IDS_ASH_AUTOCLICK_OPTION_DOUBLE_CLICK,
                                 false /* flip_for_rtl */)),
      drag_button_(
          new FloatingMenuButton(this,
                                 kAutoclickDragIcon,
                                 IDS_ASH_AUTOCLICK_OPTION_DRAG_AND_DROP,
                                 false /* flip_for_rtl */)),
      scroll_button_(new FloatingMenuButton(this,
                                            kAutoclickScrollIcon,
                                            IDS_ASH_AUTOCLICK_OPTION_SCROLL,
                                            false /* flip_for_rtl */)),
      pause_button_(new FloatingMenuButton(this,
                                           kAutoclickPauseIcon,
                                           IDS_ASH_AUTOCLICK_OPTION_NO_ACTION,
                                           false /* flip_for_rtl */)),
      position_button_(
          new FloatingMenuButton(this,
                                 kAutoclickPositionBottomLeftIcon,
                                 IDS_ASH_AUTOCLICK_OPTION_CHANGE_POSITION,
                                 false /* flip_for_rtl */,
                                 kPanelPositionButtonSize,
                                 false /* no highlight */,
                                 false /* is_a11y_togglable */)) {
  // Set view IDs for testing.
  left_click_button_->SetId(static_cast<int>(ButtonId::kLeftClick));
  right_click_button_->SetId(static_cast<int>(ButtonId::kRightClick));
  double_click_button_->SetId(static_cast<int>(ButtonId::kDoubleClick));
  drag_button_->SetId(static_cast<int>(ButtonId::kDragAndDrop));
  pause_button_->SetId(static_cast<int>(ButtonId::kPause));
  position_button_->SetId(static_cast<int>(ButtonId::kPosition));
  if (scroll_button_)
    scroll_button_->SetId(static_cast<int>(ButtonId::kScroll));

  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0);
  layout->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kEnd);
  SetLayoutManager(std::move(layout));

  // The action control buttons all have the same spacing.
  views::View* action_button_container = new views::View();
  action_button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kUnifiedMenuItemPadding,
      kUnifiedTopShortcutSpacing));
  action_button_container->AddChildView(left_click_button_);
  action_button_container->AddChildView(right_click_button_);
  action_button_container->AddChildView(double_click_button_);
  action_button_container->AddChildView(drag_button_);
  if (scroll_button_)
    action_button_container->AddChildView(scroll_button_);
  action_button_container->AddChildView(pause_button_);
  AddChildView(action_button_container);

  views::Separator* separator = new views::Separator();
  separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor));
  separator->SetPreferredHeight(kSeparatorHeight);
  int total_height = kUnifiedTopShortcutSpacing * 2 + kTrayItemSize;
  int separator_spacing = (total_height - kSeparatorHeight) / 2;
  separator->SetBorder(views::CreateEmptyBorder(
      separator_spacing - kUnifiedTopShortcutSpacing, 0, separator_spacing, 0));
  AddChildView(separator);

  views::View* position_button_container = new views::View();
  position_button_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPanelPositionButtonPadding,
                      kPanelPositionButtonPadding, kPanelPositionButtonPadding),
          kPanelPositionButtonPadding));
  position_button_container->AddChildView(position_button_);
  AddChildView(position_button_container);

  UpdateEventType(type);
  UpdatePosition(position);
}

void AutoclickMenuView::UpdateEventType(AutoclickEventType type) {
  left_click_button_->SetToggled(type == AutoclickEventType::kLeftClick);
  right_click_button_->SetToggled(type == AutoclickEventType::kRightClick);
  double_click_button_->SetToggled(type == AutoclickEventType::kDoubleClick);
  drag_button_->SetToggled(type == AutoclickEventType::kDragAndDrop);
  if (scroll_button_)
    scroll_button_->SetToggled(type == AutoclickEventType::kScroll);
  pause_button_->SetToggled(type == AutoclickEventType::kNoAction);
  if (type != AutoclickEventType::kNoAction)
    event_type_ = type;
}

void AutoclickMenuView::UpdatePosition(FloatingMenuPosition position) {
  switch (position) {
    case FloatingMenuPosition::kBottomRight:
      position_button_->SetVectorIcon(kAutoclickPositionBottomRightIcon);
      return;
    case FloatingMenuPosition::kBottomLeft:
      position_button_->SetVectorIcon(kAutoclickPositionBottomLeftIcon);
      return;
    case FloatingMenuPosition::kTopLeft:
      position_button_->SetVectorIcon(kAutoclickPositionTopLeftIcon);
      return;
    case FloatingMenuPosition::kTopRight:
      position_button_->SetVectorIcon(kAutoclickPositionTopRightIcon);
      return;
    case FloatingMenuPosition::kSystemDefault:
      position_button_->SetVectorIcon(base::i18n::IsRTL()
                                          ? kAutoclickPositionBottomLeftIcon
                                          : kAutoclickPositionBottomRightIcon);
      return;
  }
}

void AutoclickMenuView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  if (sender == position_button_) {
    FloatingMenuPosition new_position;
    // Rotate clockwise throughout the screen positions.
    switch (
        Shell::Get()->accessibility_controller()->GetAutoclickMenuPosition()) {
      case FloatingMenuPosition::kBottomRight:
        new_position = FloatingMenuPosition::kBottomLeft;
        break;
      case FloatingMenuPosition::kBottomLeft:
        new_position = FloatingMenuPosition::kTopLeft;
        break;
      case FloatingMenuPosition::kTopLeft:
        new_position = FloatingMenuPosition::kTopRight;
        break;
      case FloatingMenuPosition::kTopRight:
        new_position = FloatingMenuPosition::kBottomRight;
        break;
      case FloatingMenuPosition::kSystemDefault:
        new_position = base::i18n::IsRTL() ? FloatingMenuPosition::kTopLeft
                                           : FloatingMenuPosition::kBottomLeft;
        break;
    }
    Shell::Get()->accessibility_controller()->SetAutoclickMenuPosition(
        new_position);
    base::RecordAction(base::UserMetricsAction(
        "Accessibility.CrosAutoclick.TrayMenu.ChangePosition"));
    return;
  }
  AutoclickEventType type;
  if (sender == left_click_button_) {
    type = AutoclickEventType::kLeftClick;
  } else if (sender == right_click_button_) {
    type = AutoclickEventType::kRightClick;
  } else if (sender == double_click_button_) {
    type = AutoclickEventType::kDoubleClick;
  } else if (sender == drag_button_) {
    type = AutoclickEventType::kDragAndDrop;
  } else if (sender == scroll_button_) {
    type = AutoclickEventType::kScroll;
  } else if (sender == pause_button_) {
    // If the pause button was already selected, tapping it again turns off
    // pause and returns to the previous type.
    if (pause_button_->IsToggled())
      type = event_type_;
    else
      type = AutoclickEventType::kNoAction;
  } else {
    return;
  }

  Shell::Get()->accessibility_controller()->SetAutoclickEventType(type);
  UMA_HISTOGRAM_ENUMERATION("Accessibility.CrosAutoclick.TrayMenu.ChangeAction",
                            type);
}

const char* AutoclickMenuView::GetClassName() const {
  return "AutoclickMenuView";
}

}  // namespace ash
