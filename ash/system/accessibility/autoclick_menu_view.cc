// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_menu_view.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/accessibility/floating_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
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
                                     FloatingMenuPosition position) {
  const int total_height = kUnifiedTopShortcutSpacing * 2 + kTrayItemSize;
  const int separator_spacing = (total_height - kSeparatorHeight) / 2;
  views::Builder<AutoclickMenuView>(this)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kEnd)
      .AddChildren(
          views::Builder<views::BoxLayoutView>()
              .SetInsideBorderInsets(kUnifiedMenuItemPadding)
              .SetBetweenChildSpacing(kUnifiedTopShortcutSpacing)
              .AddChildren(views::Builder<FloatingMenuButton>()
                               .CopyAddressTo(&left_click_button_)
                               .SetID(static_cast<int>(ButtonId::kLeftClick))
                               .SetVectorIcon(kAutoclickLeftClickIcon)
                               .SetTooltipText(l10n_util::GetStringUTF16(
                                   IDS_ASH_AUTOCLICK_OPTION_LEFT_CLICK))
                               .SetCallback(base::BindRepeating(
                                   &AutoclickMenuView::OnAutoclickButtonPressed,
                                   base::Unretained(this),
                                   base::Unretained(left_click_button_))),
                           views::Builder<FloatingMenuButton>()
                               .CopyAddressTo(&right_click_button_)
                               .SetID(static_cast<int>(ButtonId::kRightClick))
                               .SetVectorIcon(kAutoclickRightClickIcon)
                               .SetTooltipText(l10n_util::GetStringUTF16(
                                   IDS_ASH_AUTOCLICK_OPTION_RIGHT_CLICK))
                               .SetCallback(base::BindRepeating(
                                   &AutoclickMenuView::OnAutoclickButtonPressed,
                                   base::Unretained(this),
                                   base::Unretained(right_click_button_))),
                           views::Builder<FloatingMenuButton>()
                               .CopyAddressTo(&double_click_button_)
                               .SetID(static_cast<int>(ButtonId::kDoubleClick))
                               .SetVectorIcon(kAutoclickDoubleClickIcon)
                               .SetTooltipText(l10n_util::GetStringUTF16(
                                   IDS_ASH_AUTOCLICK_OPTION_DOUBLE_CLICK))
                               .SetCallback(base::BindRepeating(
                                   &AutoclickMenuView::OnAutoclickButtonPressed,
                                   base::Unretained(this),
                                   base::Unretained(double_click_button_))),
                           views::Builder<FloatingMenuButton>()
                               .CopyAddressTo(&drag_button_)
                               .SetID(static_cast<int>(ButtonId::kDragAndDrop))
                               .SetVectorIcon(kAutoclickDragIcon)
                               .SetTooltipText(l10n_util::GetStringUTF16(
                                   IDS_ASH_AUTOCLICK_OPTION_DRAG_AND_DROP))
                               .SetCallback(base::BindRepeating(
                                   &AutoclickMenuView::OnAutoclickButtonPressed,
                                   base::Unretained(this),
                                   base::Unretained(drag_button_))),
                           views::Builder<FloatingMenuButton>()
                               .CopyAddressTo(&scroll_button_)
                               .SetID(static_cast<int>(ButtonId::kScroll))
                               .SetVectorIcon(kAutoclickScrollIcon)
                               .SetTooltipText(l10n_util::GetStringUTF16(
                                   IDS_ASH_AUTOCLICK_OPTION_SCROLL))
                               .SetCallback(base::BindRepeating(
                                   &AutoclickMenuView::OnAutoclickButtonPressed,
                                   base::Unretained(this),
                                   base::Unretained(scroll_button_))),
                           views::Builder<FloatingMenuButton>()
                               .CopyAddressTo(&pause_button_)
                               .SetID(static_cast<int>(ButtonId::kPause))
                               .SetVectorIcon(kAutoclickPauseIcon)
                               .SetTooltipText(l10n_util::GetStringUTF16(
                                   IDS_ASH_AUTOCLICK_OPTION_NO_ACTION))
                               .SetCallback(base::BindRepeating(
                                   &AutoclickMenuView::OnAutoclickButtonPressed,
                                   base::Unretained(this),
                                   base::Unretained(pause_button_)))),
          views::Builder<views::Separator>()
              .CopyAddressTo(&separator_)
              .SetPreferredLength(kSeparatorHeight)
              .SetColorId(ui::kColorAshSystemUIMenuSeparator)
              .SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
                  separator_spacing - kUnifiedTopShortcutSpacing, 0,
                  separator_spacing, 0))),
          views::Builder<views::BoxLayoutView>()
              .SetInsideBorderInsets(gfx::Insets::TLBR(
                  0, kPanelPositionButtonPadding, kPanelPositionButtonPadding,
                  kPanelPositionButtonPadding))
              .SetBetweenChildSpacing(kPanelPositionButtonPadding)
              .AddChildren(
                  views::Builder<FloatingMenuButton>()
                      .CopyAddressTo(&position_button_)
                      .SetID(static_cast<int>(ButtonId::kPosition))
                      .SetVectorIcon(kAutoclickPositionBottomLeftIcon)
                      .SetPreferredSize(gfx::Size(kPanelPositionButtonSize,
                                                  kPanelPositionButtonSize))
                      .SetTooltipText(l10n_util::GetStringUTF16(
                          IDS_ASH_AUTOCLICK_OPTION_CHANGE_POSITION))
                      .SetDrawHighlight(false)
                      .SetA11yTogglable(false)
                      .SetCallback(base::BindRepeating(
                          &AutoclickMenuView::OnPositionButtonPressed,
                          base::Unretained(this)))))
      .BuildChildren();
  UpdateEventType(type);
  UpdatePosition(position);
}

void AutoclickMenuView::UpdateEventType(AutoclickEventType type) {
  left_click_button_->SetToggled(type == AutoclickEventType::kLeftClick);
  right_click_button_->SetToggled(type == AutoclickEventType::kRightClick);
  double_click_button_->SetToggled(type == AutoclickEventType::kDoubleClick);
  drag_button_->SetToggled(type == AutoclickEventType::kDragAndDrop);
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

void AutoclickMenuView::OnAutoclickButtonPressed(views::Button* sender) {
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
    type = pause_button_->GetToggled() ? event_type_
                                       : AutoclickEventType::kNoAction;
  } else {
    return;
  }

  Shell::Get()->accessibility_controller()->SetAutoclickEventType(type);
  UMA_HISTOGRAM_ENUMERATION("Accessibility.CrosAutoclick.TrayMenu.ChangeAction",
                            type);
}

void AutoclickMenuView::OnPositionButtonPressed() {
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
}

BEGIN_METADATA(AutoclickMenuView)
END_METADATA

}  // namespace ash
