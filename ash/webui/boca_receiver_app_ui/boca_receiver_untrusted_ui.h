// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UNTRUSTED_UI_H_
#define ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UNTRUSTED_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class BocaReceiverUntrustedUI;

class BocaReceiverUntrustedUIConfig
    : public content::DefaultWebUIConfig<BocaReceiverUntrustedUI> {
 public:
  BocaReceiverUntrustedUIConfig();

  BocaReceiverUntrustedUIConfig(const BocaReceiverUntrustedUIConfig&) = delete;
  BocaReceiverUntrustedUIConfig& operator=(
      const BocaReceiverUntrustedUIConfig&) = delete;

  ~BocaReceiverUntrustedUIConfig() override;

  // content::DefaultWebUIConfig
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class BocaReceiverUntrustedUI : public ui::UntrustedWebUIController {
 public:
  explicit BocaReceiverUntrustedUI(content::WebUI* web_ui);

  BocaReceiverUntrustedUI(const BocaReceiverUntrustedUI&) = delete;
  BocaReceiverUntrustedUI& operator=(const BocaReceiverUntrustedUI&) = delete;

  ~BocaReceiverUntrustedUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UNTRUSTED_UI_H_
