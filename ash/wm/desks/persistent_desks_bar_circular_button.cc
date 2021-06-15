// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/persistent_desks_bar_circular_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/wm/desks/persistent_desks_bar_context_menu.h"
#include "ash/wm/overview/overview_controller.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {
constexpr int kCircularButtonSize = 32;
}  // namespace

// -----------------------------------------------------------------------------
// PersistentDesksBarCircularButton:

PersistentDesksBarCircularButton::PersistentDesksBarCircularButton(
    const gfx::VectorIcon& icon)
    : views::ImageButton(base::BindRepeating(
          &PersistentDesksBarCircularButton::OnButtonPressed,
          base::Unretained(this))),
      icon_(icon) {
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  // Keeping the same inkdrop and highlight as the buttons inside the system
  // tray menu for now since specs are not ready.
  // TODO(minch): Update once the specs are ready and need to be updated.
  TrayPopupUtils::ConfigureTrayPopupButton(this);
  views::InstallCircleHighlightPathGenerator(this);
}

gfx::Size PersistentDesksBarCircularButton::CalculatePreferredSize() const {
  return gfx::Size(kCircularButtonSize, kCircularButtonSize);
}

void PersistentDesksBarCircularButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();
  AshColorProvider::Get()->DecorateFloatingIconButton(this, icon_);
}

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

// -----------------------------------------------------------------------------
// PersistentDesksBarOverviewButton:

PersistentDesksBarOverviewButton::PersistentDesksBarOverviewButton()
    : PersistentDesksBarCircularButton(kPersistentDesksBarChevronDownIcon) {}

PersistentDesksBarOverviewButton::~PersistentDesksBarOverviewButton() = default;

void PersistentDesksBarOverviewButton::OnButtonPressed() {
  Shell::Get()->overview_controller()->StartOverview();
}

}  // namespace ash
