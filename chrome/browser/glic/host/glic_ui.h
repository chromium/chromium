// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_UI_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_UI_H_

#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace glic {
class GlicPageHandler;
class GlicFrePageHandler;
class GlicUI;

class GlicUIConfig : public content::DefaultWebUIConfig<GlicUI> {
 public:
  GlicUIConfig();
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://glic
class GlicUI : public ui::MojoWebUIController,
               public glic::mojom::PageHandlerFactory,
               public glic::mojom::FrePageHandlerFactory {
 public:
  explicit GlicUI(content::WebUI* web_ui);
  ~GlicUI() override;

  void BindInterface(
      mojo::PendingReceiver<glic::mojom::PageHandlerFactory> receiver);

  void BindInterface(
      mojo::PendingReceiver<glic::mojom::FrePageHandlerFactory> receiver);

  // When called, the UI will believe it is offline when it is launched from the
  // current test.
  static void simulate_no_connection_for_testing() {
    simulate_no_connection_ = true;
  }

 private:
  void CreatePageHandler(
      mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
      mojo::PendingRemote<glic::mojom::Page> page) override;

  void CreatePageHandler(
      mojo::PendingReceiver<glic::mojom::FrePageHandler> fre_receiver) override;

  std::unique_ptr<GlicPageHandler> page_handler_;
  std::unique_ptr<GlicFrePageHandler> fre_page_handler_;

  mojo::Receiver<glic::mojom::PageHandlerFactory> page_factory_receiver_{this};
  mojo::Receiver<glic::mojom::FrePageHandlerFactory> fre_page_factory_receiver_{
      this};

  static bool simulate_no_connection_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_UI_H_
