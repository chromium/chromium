// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_button/desk_button.h"

#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/screen_util.h"
#include "ash/shelf/desk_button_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_bar_controller.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

namespace {

constexpr int kDeskSwitchButtonWidth = 20;
constexpr int kDeskSwitchButtonHeight = 36;
constexpr int kButtonCornerRadius = 12;
constexpr int kFocusRingHaloInset = -3;
constexpr int kMaxDeskShortcut = 8;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DeskSwitchButton:
DeskSwitchButton::DeskSwitchButton(PressedCallback callback)
    : ImageButton(callback) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetSize(gfx::Size(kDeskSwitchButtonWidth, kDeskSwitchButtonHeight));
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
  layer()->SetOpacity(show ? 1.f : 0.f);
  SetEnabled(show);
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
      cros_tokens::kCrosSysSystemOnBaseOpaque, kButtonCornerRadius));
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred));

  SetupFocus(this);

  prev_desk_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kChevronSmallLeftIcon));
  prev_desk_button_->SetAccessibleName(u"Previous desk:");
  prev_desk_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysHoverOnSubtle,
      gfx::RoundedCornersF{kButtonCornerRadius, 0, 0, kButtonCornerRadius},
      /*for_border_thickness=*/0));
  SetupFocus(prev_desk_button_);

  next_desk_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kChevronSmallRightIcon));
  next_desk_button_->SetAccessibleName(u"Next desk:");
  next_desk_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysHoverOnSubtle,
      gfx::RoundedCornersF{0, kButtonCornerRadius, kButtonCornerRadius, 0},
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

  DesksController::Get()->AddObserver(this);
}

DeskButton::~DeskButton() {
  DesksController::Get()->RemoveObserver(this);
}

void DeskButton::OnExpandedStateUpdate(bool expanded) {
  is_expanded_ = expanded;
  desk_name_label_->SetText(is_expanded_ ? desk_name_ : abbreviated_desk_name_);

  // In the shrunk desk button the desk switch buttons should not be used in the
  // layout.
  prev_desk_button_->SetVisible(expanded);
  next_desk_button_->SetVisible(expanded);
  MaybeUpdateDeskSwitchButtonVisibility();

  // Re-layout the button to reflect desk switch button visibility changes.
  Layout();
}

void DeskButton::SetActivation(bool is_activated) {
  if (is_activated_ == is_activated) {
    return;
  }

  is_activated_ = is_activated;

  UpdateShelfAutoHideDisabler(disable_shelf_auto_hide_activation_,
                              !is_activated_);

  if (!force_expanded_state_) {
    if (!is_activated_ && (is_hovered_ || is_focused_)) {
      desk_button_widget_->SetExpanded(true);
    } else {
      desk_button_widget_->SetExpanded(false);
    }
  }

  SetBackground(views::CreateThemedRoundedRectBackground(
      is_activated_ ? cros_tokens::kCrosSysSystemPrimaryContainer
                    : cros_tokens::kCrosSysSystemOnBaseOpaque,
      kButtonCornerRadius));
  desk_name_label_->SetEnabledColorId(
      is_activated_ ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                    : cros_tokens::kCrosSysOnSurface);

  MaybeUpdateDeskSwitchButtonVisibility();
}

void DeskButton::SetFocused(bool is_focused) {
  if (is_focused_ == is_focused) {
    return;
  }

  is_focused_ = is_focused;
  MaybeUpdateDeskSwitchButtonVisibility();
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
  const size_t active_desk_index = desks_controller->GetActiveDeskIndex();

  return view == prev_desk_button_
             ? desks_controller->GetDeskAtIndex(active_desk_index - 1)->name()
         : view == next_desk_button_
             ? desks_controller->GetDeskAtIndex(active_desk_index + 1)->name()
             : std::u16string();
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

void DeskButton::OnMouseEntered(const ui::MouseEvent& event) {
  if (is_hovered_) {
    return;
  }

  is_hovered_ = true;

  UpdateShelfAutoHideDisabler(disable_shelf_auto_hide_hover_, !is_hovered_);

  if (is_activated_) {
    return;
  }

  if (!is_expanded_ && !force_expanded_state_) {
    // TODO(b/272383056): Would be better to have the widget register a
    // callback like "preferred_expanded_state_changed".
    desk_button_widget_->SetExpanded(true);
  }

  MaybeUpdateDeskSwitchButtonVisibility();
}

void DeskButton::OnMouseExited(const ui::MouseEvent& event) {
  if (!is_hovered_) {
    return;
  }

  is_hovered_ = false;

  UpdateShelfAutoHideDisabler(disable_shelf_auto_hide_hover_, !is_hovered_);

  if (is_activated_) {
    return;
  }

  if (is_expanded_ && !force_expanded_state_ && !is_focused_) {
    // TODO(b/272383056): Would be better to have the widget register a
    // callback like "preferred_expanded_state_changed".
    desk_button_widget_->SetExpanded(false);
  }

  MaybeUpdateDeskSwitchButtonVisibility();
}

views::View* DeskButton::GetTooltipHandlerForPoint(const gfx::Point& point) {
  // We override this function so that the tooltip manager ignores disabled desk
  // switch buttons when creating tooltips.
  views::View* tooltip_handler = Button::GetTooltipHandlerForPoint(point);
  return tooltip_handler->GetEnabled() ? tooltip_handler : this;
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

void DeskButton::AboutToRequestFocusFromTabTraversal(bool reverse) {
  Shelf::ForWindow(GetWidget()->GetNativeWindow())
      ->desk_button_widget()
      ->MaybeFocusOut(reverse);
}

void DeskButton::OnViewBlurred(views::View* observed_view) {
  MaybeContract();
}

void DeskButton::OnButtonPressed() {
  base::UmaHistogramBoolean(kDeskButtonPressesHistogramName, true);

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

  SetAccessibleName(
      l10n_util::GetStringFUTF16(IDS_SHELF_DESK_BUTTON_TITLE, desk_name_));
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
      (is_hovered_ || is_focused_) && !is_activated_ && is_expanded_;
  prev_desk_button_->SetShown(can_show_desk_switch_buttons &&
                              can_show_prev_desk_button);
  next_desk_button_->SetShown(can_show_desk_switch_buttons &&
                              can_show_next_desk_button);

  if (prev_desk_button_->GetEnabled()) {
    if ((active_desk_index - 1) <= kMaxDeskShortcut - 1) {
      prev_desk_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_SHELF_PREVIOUS_DESK_BUTTON_TITLE,
          desks_controller->GetDeskName(active_desk_index - 1),
          base::NumberToString16(active_desk_index)));
    } else {
      prev_desk_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_SHELF_PREVIOUS_DESK_BUTTON_TITLE_16_DESKS,
          desks_controller->GetDeskName(active_desk_index - 1)));
    }
  }

  if (next_desk_button_->GetEnabled()) {
    if ((active_desk_index + 1) <= kMaxDeskShortcut - 1) {
      next_desk_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_SHELF_NEXT_DESK_BUTTON_TITLE,
          desks_controller->GetDeskName(active_desk_index + 1),
          base::NumberToString16(active_desk_index + 2)));
    } else {
      next_desk_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_SHELF_NEXT_DESK_BUTTON_TITLE_16_DESKS,
          desks_controller->GetDeskName(active_desk_index + 1)));
    }
  }
}

void DeskButton::UpdateShelfAutoHideDisabler(
    absl::optional<Shelf::ScopedDisableAutoHide>& disabler,
    bool should_enable_shelf_auto_hide) {
  // If shelf is not set to always hide, no need to disable.
  if (desk_button_widget_->shelf()->auto_hide_behavior() !=
      ShelfAutoHideBehavior::kAlways) {
    return;
  }

  if (should_enable_shelf_auto_hide) {
    disabler.reset();
  } else {
    disabler.emplace(desk_button_widget_->shelf());
  }
}

void DeskButton::SetupFocus(views::Button* view) {
  view->SetInstallFocusRingOnFocus(true);
  view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  views::FocusRing::Get(view)->SetColorId(cros_tokens::kCrosSysFocusRing);
  views::InstallRoundRectHighlightPathGenerator(
      view, gfx::Insets(kFocusRingHaloInset), kButtonCornerRadius);
}

BEGIN_METADATA(DeskButton, Button)
END_METADATA

}  // namespace ash
