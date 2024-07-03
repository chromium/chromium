// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/curtain/remote_maintenance_curtain_view.h"

#include <memory>

#include "ash/assistant/ui/base/stack_layout.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/public/cpp/oobe_dialog_util.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "base/check_deref.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/view_shadow.h"

namespace ash::curtain {

namespace {

constexpr char kRemoteManagementCurtainUrl[] = "chrome://security-curtain/";

OobeDialogUtil& util() {
  return OobeDialogUtil::Get();
}

gfx::Size CalculateCurtainViewSize(const gfx::Size& size) {
  return util().CalculateDialogSize(
      size, /*shelf_height=*/0,
      /*is_horizontal=*/(size.width() >= size.height()));
}

}  // namespace

RemoteMaintenanceCurtainView::RemoteMaintenanceCurtainView() {
  Initialize();
}

RemoteMaintenanceCurtainView::~RemoteMaintenanceCurtainView() = default;

void RemoteMaintenanceCurtainView::OnBoundsChanged(const gfx::Rect&) {
  UpdateChildrenSize(size());
}

void RemoteMaintenanceCurtainView::UpdateChildrenSize(
    const gfx::Size& new_size) {
  wallpaper_view_->SetPreferredSize(new_size);
  curtain_view_->SetPreferredSize(CalculateCurtainViewSize(new_size));
}

void RemoteMaintenanceCurtainView::Initialize() {
  layout_ = SetLayoutManager(std::make_unique<StackLayout>());

  AddWallpaper();
  AddCurtainWebView();
}

void RemoteMaintenanceCurtainView::AddWallpaper() {
  // Add a copy of the wallpaper above the security curtain, since UX wants
  // to see a blurred wallpaper but we can't expose the real wallpaper as that
  // would require the security curtain to be translucent which would defeat
  // its whole purpose.
  DCHECK(!wallpaper_view_);
  wallpaper_view_ = AddChildView(
      std::make_unique<WallpaperView>(wallpaper_constants::kLockLoginBlur));
}

void RemoteMaintenanceCurtainView::AddCurtainWebView() {
  DCHECK(!curtain_view_);

  AshWebView::InitParams web_view_params;
  web_view_params.rounded_corners =
      gfx::RoundedCornersF(util().GetCornerRadius());
  curtain_view_ = AddChildView(
      AshWebViewFactory::Get()->Create(std::move(web_view_params)));

  curtain_view_->SetID(kRemoteMaintenanceCurtainAshWebViewId);
  layout_->SetVerticalAlignmentForView(curtain_view_,
                                       StackLayout::VerticalAlignment::kCenter);

  // Add a shadow
  curtain_view_shadow_ = std::make_unique<views::ViewShadow>(
      curtain_view_.get(), util().GetShadowElevation());
  curtain_view_shadow_->SetRoundedCornerRadius(util().GetCornerRadius());

  // Load the actual security curtain content.
  curtain_view_->Navigate(GURL(kRemoteManagementCurtainUrl));
}

BEGIN_METADATA(RemoteMaintenanceCurtainView)
END_METADATA

}  // namespace ash::curtain
