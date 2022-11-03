// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COLOR_INTERNALS_WALLPAPER_COLORS_HANDLER_IMPL_H_
#define ASH_WEBUI_COLOR_INTERNALS_WALLPAPER_COLORS_HANDLER_IMPL_H_

#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/webui/color_internals/mojom/color_internals.mojom.h"
#include "base/scoped_observation.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class WallpaperColorsHandlerImpl
    : public color_internals::mojom::WallpaperColorsHandler,
      public WallpaperControllerObserver {
 public:
  explicit WallpaperColorsHandlerImpl(
      mojo::PendingReceiver<ash::color_internals::mojom::WallpaperColorsHandler>
          receiver);

  WallpaperColorsHandlerImpl(const WallpaperColorsHandlerImpl&) = delete;
  WallpaperColorsHandlerImpl& operator=(const WallpaperColorsHandlerImpl&) =
      delete;

  ~WallpaperColorsHandlerImpl() override;

  // color_internals::mojom::WallpaperColorsHandler:
  void SetWallpaperColorsObserver(
      mojo::PendingRemote<color_internals::mojom::WallpaperColorsObserver>
          observer) override;

  // WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override;

 private:
  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      scoped_wallpaper_controller_observation_{this};
  mojo::Remote<color_internals::mojom::WallpaperColorsObserver>
      observer_remote_;
  mojo::Receiver<color_internals::mojom::WallpaperColorsHandler> receiver_;
};

}  // namespace ash

#endif  // ASH_WEBUI_COLOR_INTERNALS_WALLPAPER_COLORS_HANDLER_IMPL_H_
