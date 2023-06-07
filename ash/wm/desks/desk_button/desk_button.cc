// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_button/desk_button.h"

#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/screen_util.h"
#include "ash/shelf/desk_button_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/typography.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_bar_controller.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

namespace {

constexpr int kDeskSwitchButtonWidth = 20;
constexpr int kDeskSwitchButtonHeight = 36;
constexpr int kButtonCornerRadius = 12;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DeskSwitchButton:
DeskSwitchButton::DeskSwitchButton(PressedCallback callback)
    : ImageButton(callback) {
  SetSize(gfx::Size(kDeskSwitchButtonWidth, kDeskSwitchButtonHeight));
  SetImageHorizontalAlignment(
      views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
  SetImageVerticalAlignment(
      views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
  SetProperty(views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred));
  SetVisible(false);
  SetEnabled(true);
}

DeskSwitchButton::~DeskSwitchButton() = default;

void DeskSwitchButton::OnMouseEntered(const ui::MouseEvent& event) {
  if (hovered_) {
    return;
  }

  hovered_ = true;
  SchedulePaint();
}

void DeskSwitchButton::OnMouseExited(const ui::MouseEvent& event) {
  if (!hovered_) {
    return;
  }

  hovered_ = false;
  SchedulePaint();
}

void DeskSwitchButton::OnPaintBackground(gfx::Canvas* canvas) {
  if (hovered_) {
    ImageButton::OnPaintBackground(canvas);
  }
}

BEGIN_METADATA(DeskSwitchButton, views::ImageButton)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// DeskButton:
DeskButton::DeskButton(DeskButtonWidget* desk_button_widget)
    : Button(
          /*callback=*/base::BindRepeating(&DeskButton::OnButtonPressed,
                                           base::Unretained(this))),
      desk_button_widget_(desk_button_widget),
      prev_desk_button_(AddChildView(std::make_unique<DeskSwitchButton>(
          base::BindRepeating(&DeskButton::OnPreviousPressed,
                              base::Unretained(this))))),
      desk_name_label_(AddChildView(std::make_unique<views::Label>())),
      next_desk_button_(AddChildView(std::make_unique<DeskSwitchButton>(
          base::BindRepeating(&DeskButton::OnNextPressed,
                              base::Unretained(this))))) {
  SetPaintToLayer();
  SetNotifyEnterExitOnChild(true);
  layer()->SetFillsBoundsOpaquely(false);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBaseOpaque, kButtonCornerRadius));
  SetLayoutManager(std::make_unique<views::FlexLayout>());

  prev_desk_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kChevronSmallLeftIcon));
  prev_desk_button_->SetAccessibleName(u"Previous desk button");
  prev_desk_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysHoverOnSubtle,
      views::Radii{.top_left = kButtonCornerRadius,
                   .bottom_left = kButtonCornerRadius},
      /*for_border_thickness=*/0));

  next_desk_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kChevronSmallRightIcon));
  next_desk_button_->SetAccessibleName(u"Next desk button");
  next_desk_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysHoverOnSubtle,
      views::Radii{.top_right = kButtonCornerRadius,
                   .bottom_right = kButtonCornerRadius},
      /*for_border_thickness=*/0));

  CalculateDisplayNames(DesksController::Get()->active_desk());
  CHECK(!is_expanded_);

  desk_name_label_->SetText(abbreviated_desk_name_);
  desk_name_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  desk_name_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton1,
                                        *desk_name_label_);

  DesksController::Get()->AddObserver(this);
}

DeskButton::~DeskButton() {
  DesksController::Get()->RemoveObserver(this);
}

void DeskButton::OnExpandedStateUpdate(bool expanded) {
  is_expanded_ = expanded;
  desk_name_label_->SetText(is_expanded_ ? desk_name_ : abbreviated_desk_name_);
  MaybeUpdateDeskSwitchButtonVisibility();
}

void DeskButton::SetActivation(bool is_activated) {
  if (is_activated_ == is_activated) {
    return;
  }

  is_activated_ = is_activated;

  if (!force_expanded_state_) {
    if (!is_activated_ && is_hovered_) {
      desk_button_widget_->SetExpanded(true);
    } else {
      desk_button_widget_->SetExpanded(false);
    }
  }

  background()->SetNativeControlColor(GetColorProvider()->GetColor(
      is_activated_ ? cros_tokens::kCrosSysSystemPrimaryContainer
                    : cros_tokens::kCrosSysSystemOnBaseOpaque));
  desk_name_label_->SetEnabledColor(GetColorProvider()->GetColor(
      is_activated_ ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                    : cros_tokens::kCrosSysOnSurface));

  MaybeUpdateDeskSwitchButtonVisibility();
}

const std::u16string& DeskButton::GetTextForTest() const {
  return desk_name_label_->GetText();
}

void DeskButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Avoid failing accessibility checks if we don't have a name.
  Button::GetAccessibleNodeData(node_data);
  if (GetAccessibleName().empty()) {
    node_data->SetNameExplicitlyEmpty();
  }
}

void DeskButton::OnMouseEntered(const ui::MouseEvent& event) {
  if (is_hovered_) {
    return;
  }

  is_hovered_ = true;

  if (is_activated_) {
    return;
  }

  if (!is_expanded_ && !force_expanded_state_) {
    // TODO(b/272383056): Would be better to have the widget register a callback
    // like "preferred_expanded_state_changed".
    desk_button_widget_->SetExpanded(true);
  }

  MaybeUpdateDeskSwitchButtonVisibility();
}

void DeskButton::OnMouseExited(const ui::MouseEvent& event) {
  if (!is_hovered_) {
    return;
  }

  is_hovered_ = false;

  if (is_activated_) {
    return;
  }

  if (is_expanded_ && !force_expanded_state_) {
    // TODO(b/272383056): Would be better to have the widget register a callback
    // like "preferred_expanded_state_changed".
    desk_button_widget_->SetExpanded(false);
  }

  MaybeUpdateDeskSwitchButtonVisibility();
}

void DeskButton::OnDeskAdded(const Desk* desk) {
  MaybeUpdateDeskSwitchButtonVisibility();
}

void DeskButton::OnDeskRemoved(const Desk* desk) {
  MaybeUpdateDeskSwitchButtonVisibility();
}

void DeskButton::OnDeskReordered(int old_index, int new_index) {
  MaybeUpdateDeskSwitchButtonVisibility();
}

void DeskButton::OnDeskActivationChanged(const Desk* activated,
                                         const Desk* deactivated) {
  CalculateDisplayNames(activated);
  desk_name_label_->SetText(is_expanded_ ? desk_name_ : abbreviated_desk_name_);
  MaybeUpdateDeskSwitchButtonVisibility();
}

void DeskButton::OnDeskNameChanged(const Desk* desk,
                                   const std::u16string& new_name) {
  if (!desk->is_active()) {
    return;
  }

  CalculateDisplayNames(desk);
  desk_name_label_->SetText(is_expanded_ ? desk_name_ : abbreviated_desk_name_);
}

void DeskButton::OnButtonPressed() {
  aura::Window* root = desk_button_widget_->GetNativeWindow()->GetRootWindow();
  DeskBarController* desk_bar_controller =
      DesksController::Get()->desk_bar_controller();

  if (is_activated_ && desk_bar_controller->GetDeskBarView(root)) {
    desk_bar_controller->CloseDeskBar(root);
  } else {
    desk_bar_controller->OpenDeskBar(root);
  }
}

void DeskButton::OnPreviousPressed() {
  DesksController::Get()->ActivateAdjacentDesk(
      /*going_left=*/true, DesksSwitchSource::kDeskButtonSwitchButton);
  prev_desk_button_->set_hovered(false);
}

void DeskButton::OnNextPressed() {
  DesksController::Get()->ActivateAdjacentDesk(
      /*going_left=*/false, DesksSwitchSource::kDeskButtonSwitchButton);
  next_desk_button_->set_hovered(false);
}

void DeskButton::CalculateDisplayNames(const Desk* desk) {
  // Should not update desk name if desk name is empty.
  if (desk->name().empty()) {
    return;
  }

  desk_name_ = desk->name();
  base::i18n::BreakIterator iter(desk_name_,
                                 base::i18n::BreakIterator::BREAK_CHARACTER);

  if (!iter.Init()) {
    return;
  }

  iter.Advance();
  abbreviated_desk_name_ = base::i18n::ToUpper(iter.GetString());

  // If the desk name is default, then in zero state we want to show the
  // number next to the first character.
  // TODO(b/272383056): Figure out how we should abbreviate the name when
  // there are 10 or more desks. (i.e. "D16").
  if (!desk->is_name_set_by_user()) {
    abbreviated_desk_name_ += base::NumberToString16(
        DesksController::Get()->GetActiveDeskIndex() + 1);
  }
}

void DeskButton::MaybeUpdateDeskSwitchButtonVisibility() {
  DesksController* desks_controller = DesksController::Get();
  const size_t active_desk_index = desks_controller->GetActiveDeskIndex();
  const bool can_show_prev_desk_button = !(active_desk_index == 0);
  const bool can_show_next_desk_button =
      !(active_desk_index == desks_controller->desks().size() - 1);

  // There are certain conditions that indicate that we cannot show either of
  // the buttons.
  const bool can_show_desk_switch_buttons =
      is_hovered_ && !is_activated_ && is_expanded_;
  prev_desk_button_->SetVisible(can_show_desk_switch_buttons &&
                                can_show_prev_desk_button);
  next_desk_button_->SetVisible(can_show_desk_switch_buttons &&
                                can_show_next_desk_button);
}

BEGIN_METADATA(DeskButton, Button)
END_METADATA

}  // namespace ash
