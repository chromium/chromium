// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_H_
#define ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_H_

#include <memory>

#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {
namespace personalization_app {

class PersonalizationAppAmbientProvider;
class PersonalizationAppThemeProvider;
class PersonalizationAppWallpaperProvider;
class PersonalizationAppUserProvider;

class PersonalizationAppUI : public ui::MojoWebUIController {
 public:
  PersonalizationAppUI(
      content::WebUI* web_ui,
      std::unique_ptr<PersonalizationAppAmbientProvider> ambient_provider,
      std::unique_ptr<PersonalizationAppThemeProvider> theme_provider,
      std::unique_ptr<PersonalizationAppUserProvider> user_provider,
      std::unique_ptr<PersonalizationAppWallpaperProvider> wallpaper_provider);

  PersonalizationAppUI(const PersonalizationAppUI&) = delete;
  PersonalizationAppUI& operator=(const PersonalizationAppUI&) = delete;

  ~PersonalizationAppUI() override;

  void BindInterface(
      mojo::PendingReceiver<personalization_app::mojom::AmbientProvider>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<personalization_app::mojom::ThemeProvider>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<personalization_app::mojom::UserProvider> receiver);

  void BindInterface(
      mojo::PendingReceiver<personalization_app::mojom::WallpaperProvider>
          receiver);

 private:
  std::unique_ptr<PersonalizationAppAmbientProvider> ambient_provider_;
  std::unique_ptr<PersonalizationAppThemeProvider> theme_provider_;
  std::unique_ptr<PersonalizationAppUserProvider> user_provider_;
  std::unique_ptr<PersonalizationAppWallpaperProvider> wallpaper_provider_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace personalization_app
}  // namespace ash

#endif  // ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_H_
