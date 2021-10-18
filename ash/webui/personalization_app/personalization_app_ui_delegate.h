// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_DELEGATE_H_
#define ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_DELEGATE_H_

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

// Handles calling |backdrop_wallpaper_handler| code in //chrome to pass to the
// Personalization App SWA.
class PersonalizationAppUiDelegate
    : public chromeos::personalization_app::mojom::WallpaperProvider {
 public:
  virtual void BindInterface(
      mojo::PendingReceiver<
          chromeos::personalization_app::mojom::WallpaperProvider>
          receiver) = 0;
};

#endif  // ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_DELEGATE_H_
