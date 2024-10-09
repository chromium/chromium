// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SANITIZE_UI_SANITIZE_UI_H_
#define ASH_WEBUI_SANITIZE_UI_SANITIZE_UI_H_

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/sanitize_ui/mojom/sanitize_ui.mojom.h"
#include "ash/webui/sanitize_ui/sanitize_ui_delegate.h"
#include "ash/webui/sanitize_ui/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ash {
class SanitizeDialogUI;
class SanitizeSettingsResetter;

// The WebDialogUIConfig for chrome://sanitize
class SanitizeDialogUIConfig : public ChromeOSWebUIConfig<SanitizeDialogUI> {
 public:
  explicit SanitizeDialogUIConfig(
      CreateWebUIControllerFunc create_controller_func)
      : ChromeOSWebUIConfig(content::kChromeUIScheme,
                            ash::kChromeUISanitizeAppHost,
                            create_controller_func) {}
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebDialogUI for chrome://sanitize
class SanitizeDialogUI : public ui::MojoWebDialogUI {
 public:
  explicit SanitizeDialogUI(
      content::WebUI* web_ui,
      std::unique_ptr<SanitizeUIDelegate> sanitize_ui_delegate);
  ~SanitizeDialogUI() override;

  SanitizeDialogUI(const SanitizeDialogUI&) = delete;
  SanitizeDialogUI& operator=(const SanitizeDialogUI&) = delete;
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);
  void BindInterface(
      mojo::PendingReceiver<sanitize_ui::mojom::SettingsResetter> receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  // The color change handler notifies the WebUI when the color provider
  // changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  std::unique_ptr<SanitizeSettingsResetter> sanitize_settings_resetter_;
};

}  // namespace ash

#endif  // ASH_WEBUI_SANITIZE_UI_SANITIZE_UI_H_
