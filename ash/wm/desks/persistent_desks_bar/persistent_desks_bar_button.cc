// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/persistent_desks_bar/persistent_desks_bar_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/persistent_desks_bar/persistent_desks_bar_context_menu.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

constexpr int kCircularButtonSize = 32;

constexpr int kCornerRadius = 8;

}  // namespace

// -----------------------------------------------------------------------------
// PersistentDesksBarDeskButton:

PersistentDesksBarDeskButton::PersistentDesksBarDeskButton(const Desk* desk)
    : DeskButtonBase(
          desk->name(),
          /*set_text=*/true,
          base::BindRepeating(&PersistentDesksBarDeskButton::OnButtonPressed,
                              base::Unretained(this)),
          kCornerRadius),
      desk_(desk) {
  // TODO(minch): A11y of bento bar.
  SetAccessibleName(base::UTF8ToUTF16(GetClassName()));
  // Only paint the background of the active desk's button.
  SetShouldPaintBackground(desk_ == DesksController::Get()->active_desk());
}

void PersistentDesksBarDeskButton::UpdateText(std::u16string name) {
  SetText(std::move(name));
}

void PersistentDesksBarDeskButton::OnButtonPressed() {
  // If there is an ongoing desk activation, do nothing.
  DesksController* desks_controller = DesksController::Get();
  if (!desks_controller->AreDesksBeingModified()) {
    desks_controller->ActivateDesk(desk_,
                                   DesksSwitchSource::kPersistentDesksBar);
  }
}

void PersistentDesksBarDeskButton::OnThemeChanged() {
  DeskButtonBase::OnThemeChanged();
  SetEnabledTextColors(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
}

void PersistentDesksBarDeskButton::OnMouseEntered(const ui::MouseEvent& event) {
  SetShouldPaintBackground(true);
}

void PersistentDesksBarDeskButton::OnMouseExited(const ui::MouseEvent& event) {
  SetShouldPaintBackground(desk_ == DesksController::Get()->active_desk());
}

BEGIN_METADATA(PersistentDesksBarDeskButton, DeskButtonBase)
END_METADATA

// -----------------------------------------------------------------------------
// PersistentDesksBarCircularButton:

PersistentDesksBarCircularButton::PersistentDesksBarCircularButton(
    const gfx::VectorIcon& icon)
    : views::ImageButton(base::BindRepeating(
          &PersistentDesksBarCircularButton::OnButtonPressed,
          base::Unretained(this))),
      icon_(icon) {
  // TODO(minch): A11y of bento bar.
  SetAccessibleName(base::UTF8ToUTF16(GetClassName()));
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  // Keeping the same inkdrop and highlight as the buttons inside the system
  // tray menu for now since specs are not ready.
  // TODO(minch): Update once the specs are ready and need to be updated.
  StyleUtil::SetUpInkDropForButton(this);
  views::InstallCircleHighlightPathGenerator(this);
}

gfx::Size PersistentDesksBarCircularButton::CalculatePreferredSize() const {
  return gfx::Size(kCircularButtonSize, kCircularButtonSize);
}

void PersistentDesksBarCircularButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();

  const SkColor enabled_icon_color =
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kButtonIconColor);
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(icon_, enabled_icon_color));
}

BEGIN_METADATA(PersistentDesksBarCircularButton, views::ImageButton)
END_METADATA

// -----------------------------------------------------------------------------
// PersistentDesksBarVerticalDotsButton:

PersistentDesksBarVerticalDotsButton::PersistentDesksBarVerticalDotsButton()
    : PersistentDesksBarCircularButton(kPersistentDesksBarVerticalDotsIcon),
      context_menu_(
          std::make_unique<PersistentDesksBarContextMenu>(base::BindRepeating(
              &PersistentDesksBarVerticalDotsButton::OnMenuClosed,
              base::Unretained(this)))) {}

PersistentDesksBarVerticalDotsButton::~PersistentDesksBarVerticalDotsButton() =
    default;

void PersistentDesksBarVerticalDotsButton::OnButtonPressed() {
  context_menu_->ShowContextMenuForView(this, GetBoundsInScreen().CenterPoint(),
                                        ui::MENU_SOURCE_NONE);
  // Keeping InkDrop as activated after releasing the mouse with the context
  // menu opened. It will be set as deactivated after the context menu is
  // closed.
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
      views::InkDropState::ACTIVATED);
}

void PersistentDesksBarVerticalDotsButton::OnMenuClosed() {
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
      views::InkDropState::DEACTIVATED);
}

BEGIN_METADATA(PersistentDesksBarVerticalDotsButton,
               PersistentDesksBarCircularButton)
END_METADATA

// -----------------------------------------------------------------------------
// PersistentDesksBarOverviewButton:

PersistentDesksBarOverviewButton::PersistentDesksBarOverviewButton()
    : PersistentDesksBarCircularButton(kPersistentDesksBarChevronDownIcon) {}

PersistentDesksBarOverviewButton::~PersistentDesksBarOverviewButton() = default;

void PersistentDesksBarOverviewButton::OnButtonPressed() {
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kBentoBar);
}

BEGIN_METADATA(PersistentDesksBarOverviewButton,
               PersistentDesksBarCircularButton)
END_METADATA

}  // namespace ash
