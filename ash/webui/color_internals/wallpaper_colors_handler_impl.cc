// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/color_internals/wallpaper_colors_handler_impl.h"

#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/webui/color_internals/mojom/color_internals.mojom.h"
#include "base/scoped_observation.h"

namespace ash {

WallpaperColorsHandlerImpl::WallpaperColorsHandlerImpl(
    mojo::PendingReceiver<ash::color_internals::mojom::WallpaperColorsHandler>
        receiver)
    : receiver_(this, std::move(receiver)) {}

WallpaperColorsHandlerImpl::~WallpaperColorsHandlerImpl() = default;

void WallpaperColorsHandlerImpl::SetWallpaperColorsObserver(
    mojo::PendingRemote<ash::color_internals::mojom::WallpaperColorsObserver>
        observer) {
  // May already be bound if user refreshes page.
  observer_remote_.reset();
  observer_remote_.Bind(std::move(observer));
  if (!scoped_wallpaper_controller_observation_.IsObserving()) {
    scoped_wallpaper_controller_observation_.Observe(
        WallpaperController::Get());
  }
  // Call it once to initialize the observer.
  OnWallpaperColorsChanged();
}

void WallpaperColorsHandlerImpl::OnWallpaperColorsChanged() {
  DCHECK(observer_remote_.is_bound());
  const auto& calculated_colors =
      Shell::Get()->wallpaper_controller()->calculated_colors();
  if (!calculated_colors.has_value()) {
    return;
  }
  observer_remote_->OnWallpaperColorsChanged(*calculated_colors);
}

}  // namespace ash
