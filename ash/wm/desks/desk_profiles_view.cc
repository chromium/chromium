// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_profiles_view.h"

#include <cstdint>
#include <memory>

#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "base/check_op.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view.h"

namespace ash {

namespace {
// The size of desk profile avatar button.
constexpr gfx::Size kIconButtonSize(22, 22);
}  // namespace

DeskProfilesButton::DeskProfilesButton(views::Button::PressedCallback callback,
                                       Desk* desk)
    : desk_(desk) {
  desk_->AddObserver(this);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetPreferredSize(kIconButtonSize);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetSize(kIconButtonSize);
  icon_->SetImageSize(kIconButtonSize);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  focus_ring->SetPathGenerator(
      std::make_unique<views::CircleHighlightPathGenerator>(
          -gfx::Insets(focus_ring->GetHaloThickness() / 2)));
  views::InstallCircleHighlightPathGenerator(this);

  UpdateIcon();
  icon_->SetPaintToLayer();
  icon_->layer()->SetFillsBoundsOpaquely(false);
  icon_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kIconButtonSize.width()));
  // TODO(shidi):Update the accessible name if get any.
  SetAccessibleName(u"", ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
}

DeskProfilesButton::~DeskProfilesButton() {
  if (desk_) {
    desk_->RemoveObserver(this);
  }
}

void DeskProfilesButton::UpdateIcon() {
  CHECK(desk_);
  auto* delegate = Shell::Get()->GetDeskProfilesDelegate();
  CHECK(delegate);

  // Initialize Desk's Lacros profile id with primary profile id.
  const uint64_t primary_profile_id = delegate->GetPrimaryProfileId();
  if (desk_->lacros_profile_id() == 0 && primary_profile_id != 0) {
    desk_->SetLacrosProfileId(primary_profile_id);
  }
  if (auto* summary = delegate->GetProfilesSnapshotByProfileId(
          desk_->lacros_profile_id())) {
    icon_image_ = summary->icon;
    icon_->SetImage(icon_image_);
    icon_->SetTooltipText(base::UTF8ToUTF16(summary->name));
  }
}

void DeskProfilesButton::OnDeskDestroyed(const Desk* desk) {
  // Note that DeskProfilesButton's parent `DeskMiniView` might outlive the
  // `desk_`, so `desk_` need to be manually reset.
  desk_ = nullptr;
}

bool DeskProfilesButton::OnMousePressed(const ui::MouseEvent& event) {
  base::UmaHistogramBoolean(kDeskProfilesPressesHistogramName, true);
  if (event.IsLeftMouseButton()) {
    CreateMenu(event);
  }
  return ImageButton::OnMousePressed(event);
}

void DeskProfilesButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    CreateMenu(*event);
  }
}

void DeskProfilesButton::CreateMenu(const ui::LocatedEvent& event) {
  if (!desk_ || context_menu_) {
    return;
  }

  gfx::Point location_in_screen(event.location());
  View::ConvertPointToScreen(this, &location_in_screen);

  DeskActionContextMenu::Config menu_config;
  menu_config.on_context_menu_closed_callback = base::BindRepeating(
      &DeskProfilesButton::OnMenuClosed, base::Unretained(this));

  auto* delegate = Shell::Get()->GetDeskProfilesDelegate();
  CHECK(delegate);

  menu_config.anchor_position = views::MenuAnchorPosition::kBubbleBottomRight;
  menu_config.profiles = delegate->GetProfilesSnapshot();
  menu_config.current_lacros_profile_id = desk_->lacros_profile_id();
  menu_config.set_lacros_profile_id = base::BindRepeating(
      &DeskProfilesButton::OnSetLacrosProfileId, base::Unretained(this));

  context_menu_ =
      std::make_unique<DeskActionContextMenu>(std::move(menu_config));
  context_menu_->ShowContextMenuForView(
      this, location_in_screen,
      event.IsTouchEvent() ? ui::MENU_SOURCE_TOUCH : ui::MENU_SOURCE_MOUSE);
}

void DeskProfilesButton::OnMenuClosed() {
  context_menu_ = nullptr;
}

void DeskProfilesButton::OnSetLacrosProfileId(uint64_t lacros_profile_id) {
  if (desk_) {
    desk_->SetLacrosProfileId(lacros_profile_id);
  }
}

BEGIN_METADATA(DeskProfilesButton)
END_METADATA

}  // namespace ash
