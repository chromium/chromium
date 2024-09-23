// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_button/desk_switch_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/desk_button_widget.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_button/desk_button_container.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "base/strings/string_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_class_properties.h"

namespace ash {

DeskSwitchButton::DeskSwitchButton()
    : ImageButton(
          base::BindRepeating(&DeskSwitchButton::DeskSwitchButtonPressed,
                              base::Unretained(this))) {
  // Avoid failing accessibility checks if we don't have a name.
  if (GetViewAccessibility().GetCachedName().empty()) {
    GetViewAccessibility().SetName(
        "", ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  }
}

DeskSwitchButton::~DeskSwitchButton() = default;

gfx::Size DeskSwitchButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kDeskButtonSwitchButtonWidth,
                   desk_button_container_->zero_state()
                       ? kDeskButtonSwitchButtonHeightVertical
                       : kDeskButtonSwitchButtonHeightHorizontal);
}

void DeskSwitchButton::OnMouseEvent(ui::MouseEvent* event) {
  if (GetEnabled() && event->type() == ui::EventType::kMousePressed &&
      event->IsOnlyRightMouseButton()) {
    desk_button_container_->MaybeShowContextMenu(this, event);
    return;
  }

  views::ImageButton::OnMouseEvent(event);
}

void DeskSwitchButton::OnGestureEvent(ui::GestureEvent* event) {
  if (GetEnabled() && (event->type() == ui::EventType::kGestureLongPress ||
                       event->type() == ui::EventType::kGestureLongTap)) {
    desk_button_container_->MaybeShowContextMenu(this, event);
    return;
  }

  views::ImageButton::OnGestureEvent(event);
}

void DeskSwitchButton::StateChanged(ButtonState old_state) {
  if (GetState() != ButtonState::STATE_NORMAL &&
      GetState() != ButtonState::STATE_HOVERED &&
      GetState() != ButtonState::STATE_DISABLED) {
    return;
  }

  UpdateUi(DesksController::Get()->active_desk());
}

void DeskSwitchButton::AboutToRequestFocusFromTabTraversal(bool reverse) {
  desk_button_container_->desk_button_widget()->MaybeFocusOut(reverse);
}

void DeskSwitchButton::Init(DeskButtonContainer* desk_button_container,
                            Type type) {
  CHECK(desk_button_container);
  desk_button_container_ = desk_button_container;
  type_ = type;

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetImageHorizontalAlignment(
      views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
  SetImageVerticalAlignment(
      views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
  SetFlipCanvasOnPaintForRTLUI(false);

  SetInstallFocusRingOnFocus(true);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);
  views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(kDeskButtonSwitchButtonFocusRingHaloInset),
      kDeskButtonCornerRadius);

  // Use shelf view as the context menu controller so that it shows the same
  // context menu.
  set_context_menu_controller(
      desk_button_container_->shelf()->hotseat_widget()->GetShelfView());
}

std::u16string DeskSwitchButton::GetTitle() const {
  if (auto* desk_controller = DesksController::Get()) {
    int index =
        desk_controller->GetActiveDeskIndex() + (type_ == Type::kPrev ? -1 : 1);
    if (0 <= index && index < desk_controller->GetNumberOfDesks()) {
      return desk_controller->GetDeskName(index);
    }
  }
  return std::u16string();
}

void DeskSwitchButton::UpdateUi(const Desk* active_desk) {
  auto* desk_controller = DesksController::Get();
  int active_desk_index = desk_controller->GetDeskIndex(active_desk);
  if (type_ == Type::kPrev) {
    SetVisible(!desk_button_container_->zero_state() &&
               active_desk_index - 1 >= 0);
  } else {
    const int desk_count = desk_controller->GetNumberOfDesks();
    SetVisible(desk_count > 1 && !desk_button_container_->zero_state());
    SetEnabled(active_desk_index + 1 < desk_count);
  }

  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          type_ == Type::kPrev ? kChevronSmallLeftIcon : kChevronSmallRightIcon,
          GetEnabled() ? ui::kColorMenuIcon
                       : (ui::ColorId)cros_tokens::kCrosSysDisabled));

  // It's possible that the cursor is on top of this button, and button state is
  // normal, e.g. right clicks on the button.
  SetBackgroundVisible(GetEnabled() && IsMouseHovered() &&
                       GetState() == ButtonState::STATE_HOVERED);

  UpdateLocaleSpecificSettings();
}

void DeskSwitchButton::UpdateLocaleSpecificSettings() {
  const auto* desk_controller = DesksController::Get();
  const int target_index =
      desk_controller->GetActiveDeskIndex() + (type_ == Type::kPrev ? -1 : 1);
  const int desk_count = desk_controller->GetNumberOfDesks();
  if (target_index >= 0 && target_index < desk_count) {
    const Desk* target_desk = desk_controller->GetDeskAtIndex(target_index);
    const int id = type_ == Type::kPrev ? IDS_SHELF_PREVIOUS_DESK_BUTTON_TITLE
                                        : IDS_SHELF_NEXT_DESK_BUTTON_TITLE;
    GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
        id, target_desk->name(), base::NumberToString16(target_index + 1),
        base::NumberToString16(desk_count)));
  }
}

void DeskSwitchButton::DeskSwitchButtonPressed() {
  if (auto* desk_controller = DesksController::Get()) {
    const bool going_left = type_ == Type::kPrev;
    if ((going_left && !desk_controller->GetPreviousDesk()) ||
        (!going_left && !desk_controller->GetNextDesk())) {
      return;
    }
    desk_controller->ActivateAdjacentDesk(
        going_left, DesksSwitchSource::kDeskButtonSwitchButton);
  }
}

void DeskSwitchButton::SetBackgroundVisible(bool visible) {
  SetBackground(
      visible
          ? views::CreateThemedRoundedRectBackground(
                cros_tokens::kCrosSysHoverOnSubtle,
                type_ == Type::kPrev
                    ? gfx::RoundedCornersF(kDeskButtonCornerRadius,
                                           kDeskButtonSwitchButtonCornerRadius,
                                           kDeskButtonSwitchButtonCornerRadius,
                                           kDeskButtonCornerRadius)
                    : gfx::RoundedCornersF(kDeskButtonSwitchButtonCornerRadius,
                                           kDeskButtonCornerRadius,
                                           kDeskButtonCornerRadius,
                                           kDeskButtonSwitchButtonCornerRadius),
                /*for_border_thickness=*/0)
          : nullptr);
}

BEGIN_METADATA(DeskSwitchButton)
END_METADATA

}  // namespace ash
