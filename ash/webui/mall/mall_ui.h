// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MALL_MALL_UI_H_
#define ASH_WEBUI_MALL_MALL_UI_H_

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/mall/mall_page_handler.h"
#include "ash/webui/mall/mall_ui.mojom.h"
#include "ash/webui/mall/url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"
namespace ash {

class MallPageHandler;
class MallUI;

// WebUI configuration for chrome://mall.
class MallUIConfig : public ChromeOSWebUIConfig<MallUI> {
 public:
  MallUIConfig()
      : ChromeOSWebUIConfig(content::kChromeUIScheme, ash::kChromeUIMallHost) {}

  // ash::ChromeOSWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// WebUI controller for chrome://mall.
class MallUI : public ui::MojoWebUIController {
 public:
  explicit MallUI(content::WebUI* web_ui);
  MallUI(const MallUI&) = delete;
  MallUI& operator=(const MallUI&) = delete;
  ~MallUI() override;

  void BindInterface(mojo::PendingReceiver<mall::mojom::PageHandler> receiver);

 private:
  std::unique_ptr<MallPageHandler> page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_MALL_MALL_UI_H_
