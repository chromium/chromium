// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_button/desk_button.h"

#include <string>
#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/screen_util.h"
#include "ash/shelf/desk_button_widget.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_bar_controller.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/i18n/break_iterator.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// DeskSwitchButton:
DeskSwitchButton::DeskSwitchButton(PressedCallback callback)
    : ImageButton(std::move(callback)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetSize(
      gfx::Size(kDeskButtonSwitchButtonWidth, kDeskButtonSwitchButtonHeight));
  SetImageHorizontalAlignment(
      views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
  SetImageVerticalAlignment(
      views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
  SetProperty(views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred));
  SetShown(false);
  SetVisible(true);
}

DeskSwitchButton::~DeskSwitchButton() = default;

void DeskSwitchButton::SetShown(bool show) {
  layer()->SetOpacity(show ? 1.0f : 0.0f);
  SetEnabled(show);
}

bool DeskSwitchButton::GetShown() const {
  return GetEnabled() && layer()->GetTargetOpacity() == 1.0f;
}

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

void DeskSwitchButton::AboutToRequestFocusFromTabTraversal(bool reverse) {
  Shelf::ForWindow(GetWidget()->GetNativeWindow())
      ->desk_button_widget()
      ->MaybeFocusOut(reverse);
}

void DeskSwitchButton::OnViewBlurred(views::View* observed_view) {
  Shelf::ForWindow(GetWidget()->GetNativeWindow())
      ->desk_button_widget()
      ->GetDeskButton()
      ->MaybeContract();
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
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBaseOpaque, kDeskButtonCornerRadius));
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred));

  SetupFocus(this);

  // The previous desk button should always be on the left and the next desk
  // button on the right even in RTL mode to respect the direction of the desk
  // bar, which does not change in RTL either.
  if (base::i18n::IsRTL()) {
    ReorderChildView(prev_desk_button_, 3);
    ReorderChildView(next_desk_button_, 1);
  }

  prev_desk_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kChevronSmallLeftIcon));
  prev_desk_button_->SetAccessibleName(u"Previous desk:");
  prev_desk_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysHoverOnSubtle,
      gfx::RoundedCornersF{kDeskButtonCornerRadius, 0, 0,
                           kDeskButtonCornerRadius},
      /*for_border_thickness=*/0));
  SetupFocus(prev_desk_button_);

  next_desk_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kChevronSmallRightIcon));
  next_desk_button_->SetAccessibleName(u"Next desk:");
  next_desk_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysHoverOnSubtle,
      gfx::RoundedCornersF{0, kDeskButtonCornerRadius, kDeskButtonCornerRadius,
                           0},
      /*for_border_thickness=*/0));
  SetupFocus(next_desk_button_);

  CalculateDisplayNames(DesksController::Get()->active_desk());
  CHECK(!is_expanded_);

  desk_name_label_->SetText(abbreviated_desk_name_);
  desk_name_label_->SetHandlesTooltips(false);
  desk_name_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  desk_name_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  // Using color ID instead of the direct color guarantees that the label will
  // automatically change color on a theme change (b/287129850).
  desk_name_label_->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);

  // Labels automatically assume that they have an opaque background, and will
  // try to contrast with this assumed background unless we tell it not to do
  // that here.
  desk_name_label_->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton1,
                                        *desk_name_label_);

  // Use shelf view as the context menu controller so that right click on the
  // desk button shows the same context menu.
  set_context_menu_controller(
      desk_button_widget_->shelf()->hotseat_widget()->GetShelfView());

  DesksController::Get()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
}

DeskButton::~DeskButton() {
  Shell::Get()->RemoveShellObserver(this);
  DesksController::Get()->RemoveObserver(this);
}

void DeskButton::OnExpandedStateUpdate(bool expanded) {
  is_expanded_ = expanded;
  desk_name_label_->SetText(is_expanded_ ? desk_name_ : abbreviated_desk_name_);

  // In the shrunk desk button the desk switch buttons should not be used in the
  // layout.
  prev_desk_button_->SetVisible(expanded);
  next_desk_button_->SetVisible(expanded);
  MaybeUpdateDeskSwitchButtonVisibility(
      SwitchButtonUpdateSource::kDeskButtonUpdate);

  // Re-layout the button to reflect desk switch button visibility changes.
  Layout();
}

bool DeskButton::GetHovered() const {
  return GetState() == ButtonState::STATE_HOVERED;
}

void DeskButton::SetActivation(bool is_activated) {
  if (is_activated_ == is_activated) {
    return;
  }

  is_activated_ = is_activated;

  UpdateShelfAutoHideDisabler(disable_shelf_auto_hide_activation_,
                              !is_activated_);

  if (!force_expanded_state_) {
    if (!is_activated_ && (GetHovered() || is_focused_)) {
      desk_button_widget_->SetExpanded(true);
    } else {
      desk_button_widget_->SetExpanded(false);
    }
  }

  SetBackground(views::CreateThemedRoundedRectBackground(
      is_activated_ ? cros_tokens::kCrosSysSystemPrimaryContainer
                    : cros_tokens::kCrosSysSystemOnBaseOpaque,
      kDeskButtonCornerRadius));
  desk_name_label_->SetEnabledColorId(
      is_activated_ ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                    : cros_tokens::kCrosSysOnSurface);

  MaybeUpdateDeskSwitchButtonVisibility(
      SwitchButtonUpdateSource::kDeskButtonUpdate);
}

void DeskButton::SetFocused(bool is_focused) {
  if (is_focused_ == is_focused) {
    return;
  }

  is_focused_ = is_focused;
  MaybeUpdateDeskSwitchButtonVisibility(
      SwitchButtonUpdateSource::kDeskButtonUpdate);
}

void DeskButton::MaybeContract() {
  SetFocused(HasFocus() || next_desk_button_->HasFocus() ||
             prev_desk_button_->HasFocus());
  if (!is_focused_ && !force_expanded_state_) {
    desk_button_widget_->SetExpanded(false);
  }
}

std::u16string DeskButton::GetTitleForView(const views::View* view) {
  if (view == this) {
    return desk_name_;
  }

  DesksController* desks_controller = DesksController::Get();
  Desk* target_desk =
      view == prev_desk_button_   ? desks_controller->GetPreviousDesk()
      : view == next_desk_button_ ? desks_controller->GetNextDesk()
                                  : nullptr;
  return target_desk ? target_desk->name() : std::u16string();
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

  ShelfWidget* shelf_widget =
      Shelf::ForWindow(GetWidget()->GetNativeWindow())->shelf_widget();
  GetViewAccessibility().OverridePreviousFocus(
      shelf_widget->navigation_widget());
  GetViewAccessibility().OverrideNextFocus(shelf_widget);
}

void DeskButton::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED &&
      event->IsOnlyRightMouseButton()) {
    MaybeShowContextMenuForEvent(event);
    return;
  }

  views::Button::OnMouseEvent(event);
}

void DeskButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_LONG_PRESS ||
      event->type() == ui::ET_GESTURE_LONG_TAP) {
    MaybeShowContextMenuForEvent(event);
    return;
  }

  views::Button::OnGestureEvent(event);
}

void DeskButton::StateChanged(ButtonState old_state) {
  // When shell is destroying, child views under desk button will be removed
  // first, then it triggers `Button::ViewHierarchyChanged()`, as a result,
  // button state will be set to normal. In such case, it should *not* do
  // anything to the desk button widget or the chevron buttons.
  if (is_shell_destroying_ || (GetState() != ButtonState::STATE_NORMAL &&
                               GetState() != ButtonState::STATE_HOVERED)) {
    return;
  }

  UpdateShelfAutoHideDisabler(disable_shelf_auto_hide_hover_, !GetHovered());

  if (is_activated_) {
    return;
  }

  if (!force_expanded_state_ && !is_focused_) {
    desk_button_widget_->SetExpanded(GetHovered());
  }

  MaybeUpdateDeskSwitchButtonVisibility(
      SwitchButtonUpdateSource::kDeskButtonUpdate);
}

views::View* DeskButton::GetTooltipHandlerForPoint(const gfx::Point& point) {
  // We override this function so that the tooltip manager ignores disabled desk
  // switch buttons when creating tooltips.
  views::View* tooltip_handler = Button::GetTooltipHandlerForPoint(point);
  return tooltip_handler && tooltip_handler->GetEnabled() ? tooltip_handler
                                                          : this;
}

void DeskButton::OnDeskAdded(const Desk* desk, bool from_undo) {
  MaybeUpdateDeskSwitchButtonVisibility(
      SwitchButtonUpdateSource::kDeskButtonUpdate);
}

void DeskButton::OnDeskRemoved(const Desk* desk) {
  MaybeUpdateDeskSwitchButtonVisibility(
      SwitchButtonUpdateSource::kDeskButtonUpdate);
}

void DeskButton::OnDeskReordered(int old_index, int new_index) {
  MaybeUpdateDeskSwitchButtonVisibility(
      SwitchButtonUpdateSource::kDeskButtonUpdate);
}

void DeskButton::OnDeskActivationChanged(const Desk* activated,
                                         const Desk* deactivated) {
  CalculateDisplayNames(activated);
  desk_name_label_->SetText(is_expanded_ ? desk_name_ : abbreviated_desk_name_);
  MaybeUpdateDeskSwitchButtonVisibility(
      SwitchButtonUpdateSource::kDeskButtonUpdate);
}

void DeskButton::OnDeskNameChanged(const Desk* desk,
                                   const std::u16string& new_name) {
  if (!desk->is_active()) {
    return;
  }

  CalculateDisplayNames(desk);
  desk_name_label_->SetText(is_expanded_ ? desk_name_ : abbreviated_desk_name_);
}

void DeskButton::AboutToRequestFocusFromTabTraversal(bool reverse) {
  Shelf::ForWindow(GetWidget()->GetNativeWindow())
      ->desk_button_widget()
      ->MaybeFocusOut(reverse);
}

void DeskButton::OnViewBlurred(views::View* observed_view) {
  MaybeContract();
}

void DeskButton::OnShellDestroying() {
  is_shell_destroying_ = true;
}

void DeskButton::OnButtonPressed() {
  // If there is an ongoing desk switch animation, do nothing.
  DesksController* desk_controller = DesksController::Get();
  if (desk_controller->AreDesksBeingModified()) {
    return;
  }

  base::UmaHistogramBoolean(kDeskButtonPressesHistogramName, true);

  aura::Window* root = desk_button_widget_->GetNativeWindow()->GetRootWindow();
  DeskBarController* desk_bar_controller =
      desk_controller->desk_bar_controller();

  if (is_activated_ && desk_bar_controller->GetDeskBarView(root)) {
    desk_bar_controller->CloseDeskBar(root);
  } else {
    desk_bar_controller->OpenDeskBar(root);
  }
}

void DeskButton::OnPreviousPressed() {
  MaybeUpdateDeskSwitchButtonVisibility(
      SwitchButtonUpdateSource::kPreviousButtonPressed);
  DesksController::Get()->ActivateAdjacentDesk(
      /*going_left=*/true, DesksSwitchSource::kDeskButtonSwitchButton);
}

void DeskButton::OnNextPressed() {
  MaybeUpdateDeskSwitchButtonVisibility(
      SwitchButtonUpdateSource::kNextButtonPressed);
  DesksController::Get()->ActivateAdjacentDesk(
      /*going_left=*/false, DesksSwitchSource::kDeskButtonSwitchButton);
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

  if (desk->is_name_set_by_user()) {
    // For the customized desk name, it shows the first unicode for the
    // `abbreviated_desk_name_`.
    if (!iter.Advance()) {
      return;
    }
    abbreviated_desk_name_ = iter.GetString();
  } else {
    abbreviated_desk_name_ =
        u"#" + base::NumberToString16(
                   DesksController::Get()->GetActiveDeskIndex() + 1);
  }

  SetAccessibleName(
      l10n_util::GetStringFUTF16(IDS_SHELF_DESK_BUTTON_TITLE, desk_name_));
}

void DeskButton::MaybeUpdateDeskSwitchButtonVisibility(
    SwitchButtonUpdateSource source) {
  // It has to meet all the following conditions for the switch button to show:
  //   1) desk button is currently hovered or focused;
  //   2) desk button is not activated;
  //   3) desk button is expanded;
  //   4) there is a desk available for the previous/next switch button;
  DesksController* desk_controller = DesksController::Get();
  const int target_desk_index =
      desk_controller->GetActiveDeskIndex() + static_cast<int>(source);
  const int prev_desk_index = target_desk_index - 1;
  const int next_desk_index = target_desk_index + 1;
  const bool show_desk_switch_buttons =
      (GetHovered() || is_focused_) && !is_activated_ && is_expanded_;
  auto is_valid_desk_index = [desk_controller](int index) {
    return 0 <= index && index < desk_controller->GetNumberOfDesks();
  };
  const bool show_prev_desk_button =
      show_desk_switch_buttons && is_valid_desk_index(prev_desk_index);
  const bool show_next_desk_button =
      show_desk_switch_buttons && is_valid_desk_index(next_desk_index);

  prev_desk_button_->SetShown(show_prev_desk_button);
  next_desk_button_->SetShown(show_next_desk_button);

  if (show_prev_desk_button) {
    if (prev_desk_index < kDeskBarMaxDeskShortcut) {
      prev_desk_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_SHELF_PREVIOUS_DESK_BUTTON_TITLE,
          desk_controller->GetDeskName(prev_desk_index),
          base::NumberToString16(prev_desk_index + 1)));
    } else {
      prev_desk_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_SHELF_PREVIOUS_DESK_BUTTON_TITLE_16_DESKS,
          desk_controller->GetDeskName(prev_desk_index)));
    }
  }

  if (show_next_desk_button) {
    if (next_desk_index < kDeskBarMaxDeskShortcut) {
      next_desk_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_SHELF_NEXT_DESK_BUTTON_TITLE,
          desk_controller->GetDeskName(next_desk_index),
          base::NumberToString16(next_desk_index + 1)));
    } else {
      next_desk_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_SHELF_NEXT_DESK_BUTTON_TITLE_16_DESKS,
          desk_controller->GetDeskName(next_desk_index)));
    }
  }
}

void DeskButton::MaybeShowContextMenuForEvent(ui::LocatedEvent* event) {
  if (!is_activated_) {
    ui::MenuSourceType source_type = ui::MenuSourceType::MENU_SOURCE_MOUSE;
    if (event->type() == ui::ET_GESTURE_LONG_PRESS) {
      source_type = ui::MenuSourceType::MENU_SOURCE_LONG_PRESS;
    } else if (event->type() == ui::ET_GESTURE_LONG_TAP) {
      source_type = ui::MenuSourceType::MENU_SOURCE_LONG_TAP;
    }
    gfx::Point location_in_screen(event->location());
    View::ConvertPointToScreen(this, &location_in_screen);
    ShowContextMenu(location_in_screen, source_type);
    MaybeUpdateDeskSwitchButtonVisibility(
        SwitchButtonUpdateSource::kDeskButtonUpdate);
    MaybeContract();
  }

  event->SetHandled();
  event->StopPropagation();
}

void DeskButton::UpdateShelfAutoHideDisabler(
    std::optional<Shelf::ScopedDisableAutoHide>& disabler,
    bool should_enable_shelf_auto_hide) {
  if (should_enable_shelf_auto_hide) {
    disabler.reset();
  } else {
    disabler.emplace(desk_button_widget_->shelf());
  }
}

void DeskButton::SetupFocus(views::Button* view) {
  view->SetInstallFocusRingOnFocus(true);
  view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  views::FocusRing::Get(view)->SetOutsetFocusRingDisabled(true);
  views::FocusRing::Get(view)->SetColorId(cros_tokens::kCrosSysFocusRing);
  views::InstallRoundRectHighlightPathGenerator(
      view, gfx::Insets(kDeskButtonFocusRingHaloInset),
      kDeskButtonCornerRadius);
  view->SetFlipCanvasOnPaintForRTLUI(false);
}

BEGIN_METADATA(DeskButton, Button)
END_METADATA

}  // namespace ash
