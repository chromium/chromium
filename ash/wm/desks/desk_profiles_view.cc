// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_profiles_view.h"
#include <cstdint>
#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"

namespace ash {

namespace {
constexpr gfx::Size kIconButtonSize(22, 22);
}  // namespace

DeskProfilesButton::DeskProfilesButton(views::Button::PressedCallback callback,
                                       Desk* desk)
    : desk_(desk) {
  desk_->AddObserver(this);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  UpdateIcon();
  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetSize(kIconButtonSize);
  icon_->SetImageSize(kIconButtonSize);
  CHECK(!icon_image_.isNull());
  icon_->SetImage(icon_image_);
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
  }
}

void DeskProfilesButton::OnDeskDestroyed(const Desk* desk) {
  // Note that DeskProfilesButton's parent `DeskMiniView` might outlive the
  // `desk_`, so `desk_` need to be manually reset.
  desk_ = nullptr;
}

}  // namespace ash
