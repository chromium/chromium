// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_FRE_GLIC_FRE_UI_H_
#define CHROME_BROWSER_GLIC_FRE_GLIC_FRE_UI_H_

#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace glic {
class GlicFrePageHandler;
class GlicFreUI;

class GlicFreUIConfig : public content::DefaultWebUIConfig<GlicFreUI> {
 public:
  GlicFreUIConfig();
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://glic-fre
class GlicFreUI : public ui::MojoWebUIController,
                  public glic::mojom::FrePageHandlerFactory {
 public:
  explicit GlicFreUI(content::WebUI* web_ui);
  ~GlicFreUI() override;

  void BindInterface(
      mojo::PendingReceiver<glic::mojom::FrePageHandlerFactory> receiver);

 private:
  void CreatePageHandler(
      mojo::PendingReceiver<glic::mojom::FrePageHandler> receiver) override;

  std::unique_ptr<GlicFrePageHandler> fre_page_handler_;

  mojo::Receiver<glic::mojom::FrePageHandlerFactory> page_factory_receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_FRE_GLIC_FRE_UI_H_
