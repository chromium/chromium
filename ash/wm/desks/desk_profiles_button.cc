// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_profiles_button.h"

#include <cstdint>
#include <memory>

#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "base/check_op.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {
// The size of desk profile avatar button.
constexpr gfx::Size kIconButtonSize(22, 22);
}  // namespace

DeskProfilesButton::DeskProfilesButton(Desk* desk, DeskMiniView* mini_view)
    : desk_(desk), mini_view_(mini_view) {
  desk_->AddObserver(this);
  SetCallback(base::BindRepeating(&DeskProfilesButton::OnButtonPressed,
                                  base::Unretained(this)));
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetPreferredSize(kIconButtonSize);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  focus_ring->SetPathGenerator(
      std::make_unique<views::CircleHighlightPathGenerator>(
          -gfx::Insets(focus_ring->GetHaloThickness() / 2)));
  views::InstallCircleHighlightPathGenerator(this);

  LoadIconForProfile();

  GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      IDS_ASH_DESKS_DESK_PROFILES_BUTTON, desk->name()));
}

DeskProfilesButton::~DeskProfilesButton() {
  if (desk_) {
    desk_->RemoveObserver(this);
  }
}

void DeskProfilesButton::OnDeskDestroyed(const Desk* desk) {
  // Note that DeskProfilesButton's parent `DeskMiniView` might outlive the
  // `desk_`, so `desk_` need to be manually reset.
  desk_ = nullptr;
}

void DeskProfilesButton::OnDeskProfileChanged(uint64_t new_lacros_profile_id) {
  if (desk_) {
    LoadIconForProfile();
  }
}

bool DeskProfilesButton::OnMousePressed(const ui::MouseEvent& event) {
  base::UmaHistogramBoolean(kDeskProfilesPressesHistogramName, true);
  return ImageButton::OnMousePressed(event);
}

void DeskProfilesButton::AboutToRequestFocusFromTabTraversal(bool reverse) {
  if (reverse) {
    mini_view_->OnPreviewOrProfileAboutToBeFocusedByReverseTab();
  }
}

void DeskProfilesButton::OnButtonPressed(const ui::Event& event) {
  if (event.IsSynthesized() || !event.IsLocatedEvent()) {
    CreateMenu(GetBoundsInScreen().CenterPoint(), ui::MENU_SOURCE_KEYBOARD);
    return;
  }

  gfx::Point location_in_screen(event.AsLocatedEvent()->location());
  views::View::ConvertPointToScreen(this, &location_in_screen);
  CreateMenu(location_in_screen, event.IsMouseEvent() ? ui::MENU_SOURCE_MOUSE
                                                      : ui::MENU_SOURCE_TOUCH);
}

void DeskProfilesButton::LoadIconForProfile() {
  CHECK(desk_);

  auto* delegate = Shell::Get()->GetDeskProfilesDelegate();
  CHECK(delegate);

  if (auto* summary = delegate->GetProfilesSnapshotByProfileId(
          desk_->lacros_profile_id())) {
    SetImageModel(
        ButtonState::STATE_NORMAL,
        ui::ImageModel::FromImageSkia(
            gfx::ImageSkiaOperations::CreateCroppedCenteredRoundRectImage(
                kIconButtonSize, kIconButtonSize.width() / 2, summary->icon)));
    SetTooltipText(summary->name);
  } else {
    SetImageModel(ButtonState::STATE_NORMAL, ui::ImageModel());
    SetTooltipText(std::u16string());
  }
}

void DeskProfilesButton::CreateMenu(gfx::Point location_in_screen,
                                    ui::MenuSourceType menu_source) {
  if (!desk_ || context_menu_) {
    return;
  }

  DeskActionContextMenu::Config menu_config;
  menu_config.on_context_menu_closed_callback = base::BindRepeating(
      &DeskProfilesButton::OnMenuClosed, base::Unretained(this));

  auto* delegate = Shell::Get()->GetDeskProfilesDelegate();
  CHECK(delegate);

  menu_config.anchor_position = views::MenuAnchorPosition::kBubbleBottomRight;
  menu_config.profiles = delegate->GetProfilesSnapshot();
  menu_config.current_lacros_profile_id =
      delegate->ResolveProfileId(desk_->lacros_profile_id());
  menu_config.set_lacros_profile_id = base::BindRepeating(
      &DeskProfilesButton::OnSetLacrosProfileId, base::Unretained(this));

  context_menu_ = std::make_unique<DeskActionContextMenu>(
      std::move(menu_config), mini_view_);
  context_menu_->ShowContextMenuForView(this, location_in_screen, menu_source);
}

void DeskProfilesButton::OnMenuClosed() {
  context_menu_ = nullptr;

  // Move focus back to the profile button when the menu has closed. This
  // ensures that the ChromeVox focus ring doesn't float in empty space after
  // the menu has been dismissed.
  RequestFocus();
}

void DeskProfilesButton::OnSetLacrosProfileId(uint64_t lacros_profile_id) {
  if (desk_) {
    desk_->SetLacrosProfileId(
        lacros_profile_id, DeskProfilesSelectProfileSource::kDeskProfileButton);
  }
}

BEGIN_METADATA(DeskProfilesButton)
END_METADATA

}  // namespace ash
