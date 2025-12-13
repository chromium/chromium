// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UI_H_
#define ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class BocaReceiverUI;

class BocaReceiverUIConfig
    : public content::DefaultWebUIConfig<BocaReceiverUI> {
 public:
  BocaReceiverUIConfig();

  BocaReceiverUIConfig(const BocaReceiverUIConfig&) = delete;
  BocaReceiverUIConfig& operator=(const BocaReceiverUIConfig&) = delete;

  ~BocaReceiverUIConfig() override;

  // content::DefaultWebUIConfig
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class BocaReceiverUI : public content::WebUIController {
 public:
  explicit BocaReceiverUI(content::WebUI* web_ui);

  BocaReceiverUI(const BocaReceiverUI&) = delete;
  BocaReceiverUI& operator=(const BocaReceiverUI&) = delete;

  ~BocaReceiverUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UI_H_
