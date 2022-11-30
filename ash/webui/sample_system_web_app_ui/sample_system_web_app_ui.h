// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_SYSTEM_WEB_APP_UI_H_
#define ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_SYSTEM_WEB_APP_UI_H_

#if defined(OFFICIAL_BUILD)
#error Sample System Web App should only be included in unofficial builds.
#endif

#include <memory>

#include "ash/webui/sample_system_web_app_ui/mojom/sample_system_web_app_ui.mojom.h"
#include "ash/webui/sample_system_web_app_ui/sample_page_handler.h"
#include "ash/webui/sample_system_web_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}

namespace ash {

class SampleSystemWebAppUI;

// The WebUIConfig for chrome://sample-system-web-app/.
class SampleSystemWebAppUIConfig
    : public SystemWebAppUIConfig<SampleSystemWebAppUI> {
 public:
  SampleSystemWebAppUIConfig()
      : SystemWebAppUIConfig(kChromeUISampleSystemWebAppHost,
                             SystemWebAppType::SAMPLE) {}
};

class SampleSystemWebAppUI : public ui::MojoWebUIController,
                             public mojom::sample_swa::PageHandlerFactory {
 public:
  explicit SampleSystemWebAppUI(content::WebUI* web_ui);
  SampleSystemWebAppUI(const SampleSystemWebAppUI&) = delete;
  SampleSystemWebAppUI& operator=(const SampleSystemWebAppUI&) = delete;
  ~SampleSystemWebAppUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::sample_swa::PageHandlerFactory> factory);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  void CreateParentPage(
      mojo::PendingRemote<mojom::sample_swa::ChildUntrustedPage> child_page,
      mojo::PendingReceiver<mojom::sample_swa::ParentTrustedPage> parent_page);

 private:
  // mojom::sample_swa::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<mojom::sample_swa::PageHandler> handler,
      mojo::PendingRemote<mojom::sample_swa::Page> page) override;

  mojo::Receiver<mojom::sample_swa::PageHandlerFactory> sample_page_factory_{
      this};

  // Handles requests from the user visible page. Created when navigating to the
  // WebUI page, should live as long as the WebUIController. In most cases this
  // matches the lifetime of the page. If the WebUIController is re-used for
  // same-origin navigations, it is recreated when the navigation commits.
  std::unique_ptr<PageHandler> sample_page_handler_;

  // The color change handler notifies the WebUI when the color provider
  // changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  // Called navigating to a WebUI page to create page handler.
  void WebUIPrimaryPageChanged(content::Page& page) override;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_SYSTEM_WEB_APP_UI_H_
