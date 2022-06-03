// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_UNTRUSTED_PERSONALIZATION_APP_UI_CONFIG_H_
#define ASH_WEBUI_PERSONALIZATION_APP_UNTRUSTED_PERSONALIZATION_APP_UI_CONFIG_H_

#include "ui/webui/untrusted_web_ui_controller.h"
#include "ui/webui/webui_config.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

class UntrustedPersonalizationAppUIConfig : public ui::WebUIConfig {
 public:
  UntrustedPersonalizationAppUIConfig();
  ~UntrustedPersonalizationAppUIConfig() override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;
};

}  // namespace ash

#endif  // ASH_WEBUI_PERSONALIZATION_APP_UNTRUSTED_PERSONALIZATION_APP_UI_CONFIG_H_
