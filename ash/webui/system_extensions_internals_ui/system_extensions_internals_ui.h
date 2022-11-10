// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SYSTEM_EXTENSIONS_INTERNALS_UI_SYSTEM_EXTENSIONS_INTERNALS_UI_H_
#define ASH_WEBUI_SYSTEM_EXTENSIONS_INTERNALS_UI_SYSTEM_EXTENSIONS_INTERNALS_UI_H_

#include <memory>

#include "ash/webui/system_extensions_internals_ui/mojom/system_extensions_internals_ui.mojom.h"
#include "ash/webui/system_extensions_internals_ui/url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class SystemExtensionsInternalsUI;

// WebUIConfig for chrome://system-extensions-internals
class SystemExtensionsInternalsUIConfig
    : public content::DefaultWebUIConfig<SystemExtensionsInternalsUI> {
 public:
  SystemExtensionsInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           ash::kChromeUISystemExtensionsInternalsHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// WebUIController for chrome://system-extensions-internals/.
class SystemExtensionsInternalsUI : public ui::MojoWebUIController {
 public:
  explicit SystemExtensionsInternalsUI(content::WebUI* web_ui);
  SystemExtensionsInternalsUI(const SystemExtensionsInternalsUI&) = delete;
  SystemExtensionsInternalsUI& operator=(const SystemExtensionsInternalsUI&) =
      delete;
  ~SystemExtensionsInternalsUI() override;

  // Implemented in //chrome/browser/chrome_browser_interface_binders.cc
  // because PageHandler is implemented in //chrome/browser.
  void BindInterface(
      mojo::PendingReceiver<mojom::system_extensions_internals::PageHandler>
          page_handler);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  std::unique_ptr<mojom::system_extensions_internals::PageHandler>
      page_handler_;
};

}  // namespace ash

#endif  // ASH_WEBUI_SYSTEM_EXTENSIONS_INTERNALS_UI_SYSTEM_EXTENSIONS_INTERNALS_UI_H_
