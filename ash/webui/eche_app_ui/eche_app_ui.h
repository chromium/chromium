// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_APP_UI_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_APP_UI_H_

#include "ash/webui/eche_app_ui/mojom/eche_app.mojom-forward.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "ash/webui/eche_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash::eche_app {

class EcheAppManager;
class EcheAppUI;

// The WebUIConfig for chrome://eche-app/.
class EcheAppUIConfig : public SystemWebAppUIConfig<EcheAppUI> {
 public:
  explicit EcheAppUIConfig(
      SystemWebAppUIConfig::CreateWebUIControllerFunc create_controller_func)
      : SystemWebAppUIConfig(ash::eche_app::kChromeUIEcheAppHost,
                             SystemWebAppType::ECHE,
                             create_controller_func) {}
};

// The WebUI for chrome://eche-app/.
class EcheAppUI : public ui::MojoWebUIController {
 public:
  EcheAppUI(content::WebUI* web_ui, EcheAppManager* manager);
  EcheAppUI(const EcheAppUI&) = delete;
  EcheAppUI& operator=(const EcheAppUI&) = delete;
  ~EcheAppUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver);

  void BindInterface(mojo::PendingReceiver<mojom::SystemInfoProvider> receiver);

  void BindInterface(
      mojo::PendingReceiver<mojom::AccessibilityProvider> receiver);

  void BindInterface(mojo::PendingReceiver<mojom::UidGenerator> receiver);

  void BindInterface(
      mojo::PendingReceiver<mojom::NotificationGenerator> receiver);

  void BindInterface(
      mojo::PendingReceiver<mojom::DisplayStreamHandler> receiver);

  void BindInterface(
      mojo::PendingReceiver<mojom::StreamOrientationObserver> receiver);

  void BindInterface(
      mojo::PendingReceiver<mojom::ConnectionStatusObserver> receiver);

  void BindInterface(
      mojo::PendingReceiver<mojom::KeyboardLayoutHandler> receiver);

 private:
  raw_ptr<EcheAppManager> manager_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::eche_app

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_APP_UI_H_
