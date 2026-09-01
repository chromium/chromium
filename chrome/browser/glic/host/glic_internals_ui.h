// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_INTERNALS_UI_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_INTERNALS_UI_H_

#include <memory>

#include "chrome/browser/glic/host/glic_internals.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace glic {

class GlicInternalsPageHandler;

// The WebUIController for chrome://glic/internals.
class GlicInternalsUI : public ui::MojoWebUIController,
                        public mojom::InternalsPageHandlerFactory {
 public:
  explicit GlicInternalsUI(content::WebUI* web_ui);
  GlicInternalsUI(const GlicInternalsUI&) = delete;
  GlicInternalsUI& operator=(const GlicInternalsUI&) = delete;
  ~GlicInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::InternalsPageHandlerFactory> receiver);

  // mojom::InternalsPageHandlerFactory:
  void CreateInternalsPageHandler(
      mojo::PendingReceiver<mojom::InternalsPageHandler> receiver) override;

 private:
  std::unique_ptr<GlicInternalsPageHandler> page_handler_;
  mojo::Receiver<mojom::InternalsPageHandlerFactory> page_factory_receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_INTERNALS_UI_H_
