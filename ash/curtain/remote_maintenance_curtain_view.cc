// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/curtain/remote_maintenance_curtain_view.h"

#include <memory>

#include "ash/assistant/ui/base/stack_layout.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "base/check_deref.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace ash::curtain {

namespace {

constexpr char kRemoteManagementCurtainUrl[] = "chrome://security-curtain/";

gfx::Size CalculateCurtainViewSize(const gfx::Size& size) {
  // TODO(b/271099991): Use correct margins once Oobe code has migrated to the
  // new UX style so we can use their code.
  const int horizontal_margin = size.width() / 10;
  const int vertical_margin = size.height() / 10;
  return gfx::Size(size.width() - 2 * horizontal_margin,
                   size.height() - 2 * vertical_margin);
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
  curtain_view_ =
      AddChildView(AshWebViewFactory::Get()->Create(AshWebView::InitParams()));
  layout_->SetVerticalAlignmentForView(curtain_view_,
                                       StackLayout::VerticalAlignment::kCenter);

  // Load the actual security curtain content.
  curtain_view_->Navigate(GURL(kRemoteManagementCurtainUrl));
}

}  // namespace ash::curtain
