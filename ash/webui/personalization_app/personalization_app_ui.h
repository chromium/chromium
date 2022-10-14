// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_H_
#define ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_H_

#include <memory>

#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ui {
class ColorChangeHandler;
}  // namespace ui

namespace ash::personalization_app {

class PersonalizationAppAmbientProvider;
class PersonalizationAppKeyboardBacklightProvider;
class PersonalizationAppThemeProvider;
class PersonalizationAppWallpaperProvider;
class PersonalizationAppUserProvider;

class PersonalizationAppUI : public ui::MojoWebUIController {
 public:
  PersonalizationAppUI(
      content::WebUI* web_ui,
      std::unique_ptr<PersonalizationAppAmbientProvider> ambient_provider,
      std::unique_ptr<PersonalizationAppKeyboardBacklightProvider>
          keyboard_backlight_provider,
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
      mojo::PendingReceiver<
          personalization_app::mojom::KeyboardBacklightProvider> receiver);

  void BindInterface(
      mojo::PendingReceiver<personalization_app::mojom::ThemeProvider>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<personalization_app::mojom::UserProvider> receiver);

  void BindInterface(
      mojo::PendingReceiver<personalization_app::mojom::WallpaperProvider>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  void AddBooleans(content::WebUIDataSource* source);

  void HandleWebUIRequest(const std::string& path,
                          content::WebUIDataSource::GotDataCallback callback);

  std::unique_ptr<PersonalizationAppAmbientProvider> ambient_provider_;
  std::unique_ptr<PersonalizationAppKeyboardBacklightProvider>
      keyboard_backlight_provider_;
  std::unique_ptr<PersonalizationAppThemeProvider> theme_provider_;
  std::unique_ptr<PersonalizationAppUserProvider> user_provider_;
  std::unique_ptr<PersonalizationAppWallpaperProvider> wallpaper_provider_;
  // The color change handler notifies the WebUI when the color provider
  // changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  base::WeakPtrFactory<PersonalizationAppUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::personalization_app

#endif  // ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_UI_H_
