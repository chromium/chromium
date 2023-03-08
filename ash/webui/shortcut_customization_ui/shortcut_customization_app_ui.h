// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_SHORTCUT_CUSTOMIZATION_APP_UI_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_SHORTCUT_CUSTOMIZATION_APP_UI_H_

#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

class ShortcutCustomizationAppUI;

// The WebUIConfig for chrome://shortcut-customization.
class ShortcutCustomizationAppUIConfig
    : public SystemWebAppUIConfig<ShortcutCustomizationAppUI> {
 public:
  ShortcutCustomizationAppUIConfig()
      : SystemWebAppUIConfig(kChromeUIShortcutCustomizationAppHost,
                             SystemWebAppType::SHORTCUT_CUSTOMIZATION) {}
};

class ShortcutCustomizationAppUI : public ui::MojoWebUIController {
 public:
  explicit ShortcutCustomizationAppUI(content::WebUI* web_ui);
  ShortcutCustomizationAppUI(const ShortcutCustomizationAppUI&) = delete;
  ShortcutCustomizationAppUI& operator=(const ShortcutCustomizationAppUI&) =
      delete;
  ~ShortcutCustomizationAppUI() override;

  void BindInterface(
      mojo::PendingReceiver<
          shortcut_customization::mojom::AcceleratorConfigurationProvider>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<shortcut_customization::mojom::SearchHandler>
          receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_SHORTCUT_CUSTOMIZATION_APP_UI_H_
