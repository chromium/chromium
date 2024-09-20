// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_GROWTH_INTERNALS_GROWTH_INTERNALS_UI_H_
#define ASH_WEBUI_GROWTH_INTERNALS_GROWTH_INTERNALS_UI_H_

#include <memory>

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/growth_internals/constants.h"
#include "ash/webui/growth_internals/growth_internals.mojom-forward.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class GrowthInternalsPageHandler;
class GrowthInternalsUI;

class GrowthInternalsUIConfig : public ChromeOSWebUIConfig<GrowthInternalsUI> {
 public:
  GrowthInternalsUIConfig()
      : ChromeOSWebUIConfig<GrowthInternalsUI>(content::kChromeUIScheme,
                                               kGrowthInternalsHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class GrowthInternalsUI : public ui::MojoWebUIController {
 public:
  explicit GrowthInternalsUI(content::WebUI* web_ui);
  ~GrowthInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<growth::mojom::PageHandler> receiver);

 private:
  std::unique_ptr<GrowthInternalsPageHandler> page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_GROWTH_INTERNALS_GROWTH_INTERNALS_UI_H_
