// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_BOCA_UI_H_
#define ASH_WEBUI_BOCA_UI_BOCA_UI_H_

#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ash {
class BocaUI;

// WebUI config for Boca SWA.
class BocaUIConfig : public SystemWebAppUIConfig<BocaUI> {
 public:
  BocaUIConfig()
      : SystemWebAppUIConfig(kChromeBocaAppHost, SystemWebAppType::BOCA) {}
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://boca-app/.
class BocaUI : public ui::MojoWebUIController {
 public:
  explicit BocaUI(content::WebUI* web_ui);
  BocaUI(const BocaUI&) = delete;
  BocaUI& operator=(const BocaUI&) = delete;
  ~BocaUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_BOCA_UI_BOCA_UI_H_
