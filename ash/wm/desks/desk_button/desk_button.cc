// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_button/desk_button.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
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
#include "ash/wm/desks/desk_button/desk_button_container.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// DeskButton:
DeskButton::DeskButton()
    : views::Button(base::BindRepeating(&DeskButton::OnButtonPressed,
                                        base::Unretained(this))) {
  // Avoid failing accessibility checks if we don't have a name.
  if (GetViewAccessibility().GetCachedName().empty()) {
    GetViewAccessibility().SetName(
        "", ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  }
}

DeskButton::~DeskButton() {}

void DeskButton::SetZeroState(bool zero_state) {
  zero_state_ = zero_state;
  UpdateBackground();
}

gfx::Size DeskButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (zero_state_) {
    return {kDeskButtonWidthVertical, kDeskButtonHeightVertical};
  }

  int height = kDeskButtonHeightHorizontal;
  int width =
      GetButtonInsets().width() +
      desk_name_label_
          ->GetPreferredSize(views::SizeBounds(desk_name_label_->width(), {}))
          .width();
    width = std::clamp(width, kDeskButtonWidthHorizontalZeroNoAvatar,
                       kDeskButtonWidthHorizontalExpandedNoAvatar);

  return {width, height};
}

void DeskButton::Layout(PassKey) {
  if (!desk_button_container_) {
    return;
  }

  LayoutSuperclass<views::Button>(this);

  // Layout when it's vertical shelf.
  gfx::Rect available_bounds = gfx::Rect(size());
  if (desk_button_container_->zero_state()) {
    available_bounds.Inset(GetButtonInsets());
    desk_name_label_->SetBoundsRect(available_bounds);
    return;
  }

  // Layout when the desk avatar is *not* visible.
  available_bounds.Inset(GetButtonInsets());
  desk_name_label_->SetBoundsRect(available_bounds);
}

void DeskButton::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMousePressed &&
      event->IsOnlyRightMouseButton()) {
    desk_button_container_->MaybeShowContextMenu(this, event);
    return;
  }

  views::Button::OnMouseEvent(event);
}

void DeskButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureLongPress ||
      event->type() == ui::EventType::kGestureLongTap) {
    desk_button_container_->MaybeShowContextMenu(this, event);
    return;
  }

  views::Button::OnGestureEvent(event);
}

void DeskButton::AboutToRequestFocusFromTabTraversal(bool reverse) {
  desk_button_container_->desk_button_widget()->MaybeFocusOut(reverse);
}

void DeskButton::Init(DeskButtonContainer* desk_button_container) {
  CHECK(desk_button_container);
  desk_button_container_ = desk_button_container;

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetFlipCanvasOnPaintForRTLUI(false);
  UpdateBackground();

  SetInstallFocusRingOnFocus(true);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);
  views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(kDeskButtonFocusRingHaloInset),
      kDeskButtonCornerRadius);

  AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&desk_name_label_)
          .SetHandlesTooltips(false)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER)
          .SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE)
          .SetEnabledColor(cros_tokens::kCrosSysOnSurface)
          .SetAutoColorReadabilityEnabled(false)
          .Build());
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                        *desk_name_label_);

  // Use shelf view as the context menu controller so that it shows the same
  // context menu.
  set_context_menu_controller(
      desk_button_container_->shelf()->hotseat_widget()->GetShelfView());
}

void DeskButton::SetActivation(bool is_activated) {
  if (is_activated_ == is_activated) {
    return;
  }

  is_activated_ = is_activated;

  UpdateBackground();
  desk_name_label_->SetEnabledColor(
      is_activated_ ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                    : cros_tokens::kCrosSysOnSurface);

  UpdateShelfAutoHideDisabler(disable_shelf_auto_hide_activation_,
                              !is_activated_);
}

std::u16string DeskButton::GetTitle() const {
  return DesksController::Get()->active_desk()->name();
}

gfx::Insets DeskButton::GetButtonInsets() const {
  if (desk_button_container_->zero_state()) {
    return kDeskButtonInsetVerticalNoAvatar;
  }

  return kDeskButtonInsetHorizontalExpandedNoAvatar;
}

void DeskButton::UpdateUi(const Desk* active_desk) {
  UpdateLocaleSpecificSettings();
}

void DeskButton::UpdateLocaleSpecificSettings() {
  // Update the accessible name.
  DesksController* desk_controller = DesksController::Get();
  const Desk* active_desk = desk_controller->active_desk();
  GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      IDS_SHELF_DESK_BUTTON_TITLE_NO_PROFILE_AVATAR, active_desk->name(),
      base::NumberToString16(desk_controller->GetDeskIndex(active_desk) + 1),
      base::NumberToString16(desk_controller->GetNumberOfDesks())));

  // Update the button text since the default desk name can be locale specific.
  desk_name_label_->SetText(GetDeskNameLabelText(active_desk));
}

void DeskButton::UpdateAccessiblePreviousAndNextFocus() {
  if (GetWidget() && GetWidget()->GetNativeWindow()) {
    ShelfWidget* shelf_widget =
        Shelf::ForWindow(GetWidget()->GetNativeWindow())->shelf_widget();
    GetViewAccessibility().SetPreviousFocus(shelf_widget->navigation_widget());
    GetViewAccessibility().SetNextFocus(shelf_widget);
  } else {
    GetViewAccessibility().SetPreviousFocus(nullptr);
    GetViewAccessibility().SetNextFocus(nullptr);
  }
}

void DeskButton::OnButtonPressed() {
  // If there is an ongoing desk switch animation, do nothing.
  DesksController* desk_controller = DesksController::Get();
  if (desk_controller->AreDesksBeingModified()) {
    return;
  }

  base::UmaHistogramBoolean(kDeskButtonPressesHistogramName, true);

  aura::Window* root = GetWidget()->GetNativeWindow()->GetRootWindow();
  DeskBarController* desk_bar_controller =
      desk_controller->desk_bar_controller();

  if (is_activated_ && desk_bar_controller->GetDeskBarView(root)) {
    desk_bar_controller->CloseDeskBar(root);
  } else {
    desk_bar_controller->OpenDeskBar(root);
  }
}

std::u16string DeskButton::GetDeskNameLabelText(const Desk* active_desk) const {
  int active_desk_index = DesksController::Get()->GetDeskIndex(active_desk);
  if (active_desk->name().empty() || active_desk_index < 0) {
    return std::u16string();
  }

  if (zero_state_) {
    base::i18n::BreakIterator iter(active_desk->name(),
                                   base::i18n::BreakIterator::BREAK_CHARACTER);
    if (!iter.Init()) {
      return std::u16string();
    }
    if (active_desk->is_name_set_by_user()) {
      return iter.Advance() ? std::u16string(iter.GetString())
                            : std::u16string();
    }
    return u"#" + base::NumberToString16(active_desk_index + 1);
  }

  return active_desk->name();
}

void DeskButton::UpdateShelfAutoHideDisabler(
    std::optional<Shelf::ScopedDisableAutoHide>& disabler,
    bool should_enable_shelf_auto_hide) {
  if (should_enable_shelf_auto_hide) {
    disabler.reset();
  } else {
    disabler.emplace(desk_button_container_->shelf());
  }
}

void DeskButton::UpdateBackground() {
  SetBackground(views::CreateRoundedRectBackground(
      is_activated_ ? cros_tokens::kCrosSysSystemPrimaryContainer
                    : (zero_state_ ? cros_tokens::kCrosSysSystemOnBase
                                   : cros_tokens::kCrosSysSystemOnBase1),
      kDeskButtonCornerRadius));
}

BEGIN_METADATA(DeskButton)
END_METADATA

}  // namespace ash
