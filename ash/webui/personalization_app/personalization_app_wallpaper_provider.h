// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_WALLPAPER_PROVIDER_H_
#define ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_WALLPAPER_PROVIDER_H_

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::personalization_app {

// Handles calling |backdrop_wallpaper_handler| code in //chrome to pass to the
// Personalization App SWA.
class PersonalizationAppWallpaperProvider : public mojom::WallpaperProvider {
 public:
  virtual void BindInterface(
      mojo::PendingReceiver<mojom::WallpaperProvider> receiver) = 0;

  virtual void GetWallpaperAsJpegBytes(
      content::WebUIDataSource::GotDataCallback callback) = 0;

  // Not all users that can view the personalization app can also see google
  // photos. Users without a gaia account cannot use the photos APIs.
  virtual bool IsEligibleForGooglePhotos() = 0;
};

}  // namespace ash::personalization_app

#endif  // ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_WALLPAPER_PROVIDER_H_
