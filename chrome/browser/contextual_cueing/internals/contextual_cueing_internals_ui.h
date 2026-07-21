// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_INTERNALS_CONTEXTUAL_CUEING_INTERNALS_UI_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_INTERNALS_CONTEXTUAL_CUEING_INTERNALS_UI_H_

#include "chrome/browser/contextual_cueing/internals/contextual_cueing_internals.mojom-forward.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class BrowserContext;
}

namespace contextual_cueing_internals {

class ContextualCueingInternalsUI;

class ContextualCueingInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<ContextualCueingInternalsUI> {
 public:
   ContextualCueingInternalsUIConfig()
       : DefaultInternalWebUIConfig(
             chrome::kChromeUIContextualCueingInternalsHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class ContextualCueingInternalsPageHandler;

class ContextualCueingInternalsUI : public ui::MojoWebUIController {
 public:
  explicit ContextualCueingInternalsUI(content::WebUI* web_ui);
  ~ContextualCueingInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<contextual_cueing_internals::mojom::PageHandler>
          receiver);

  WEB_UI_CONTROLLER_TYPE_DECL();

 private:
  std::unique_ptr<ContextualCueingInternalsPageHandler> page_handler_;
};

}  // namespace contextual_cueing_internals

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_INTERNALS_CONTEXTUAL_CUEING_INTERNALS_UI_H_
