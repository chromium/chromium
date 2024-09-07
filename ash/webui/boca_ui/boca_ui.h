// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_BOCA_UI_H_
#define ASH_WEBUI_BOCA_UI_BOCA_UI_H_

#include <memory>

#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ui {
class ColorChangeHandler;
}  // namespace ui

namespace ash::boca {
class BocaUI;
class BocaAppHandler;

// The WebUI for chrome-untrusted://boca-app/. Boca app is directly served in
// main frame.
class BocaUI : public ui::UntrustedWebUIController,
               public boca::mojom::BocaPageHandlerFactory {
 public:
  explicit BocaUI(content::WebUI* web_ui);
  BocaUI(const BocaUI&) = delete;
  BocaUI& operator=(const BocaUI&) = delete;
  ~BocaUI() override;
  void BindInterface(
      mojo::PendingReceiver<boca::mojom::BocaPageHandlerFactory> factory);

  // This handler grabs a reference to the page and pushes a colorChangeEvent
  // to the untrusted JS running there when the OS color scheme has changed.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  // ash::boca::mojom ::BocaPageHandlerFactory:
  void Create(mojo::PendingReceiver<boca::mojom::PageHandler> page_handler,
              mojo::PendingRemote<boca::mojom::Page> page) override;

 private:
  raw_ptr<content::WebUI> web_ui_;
  mojo::Receiver<boca::mojom::BocaPageHandlerFactory> receiver_{this};
  std::unique_ptr<BocaAppHandler> page_handler_impl_;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::boca

#endif  // ASH_WEBUI_BOCA_UI_BOCA_UI_H_
