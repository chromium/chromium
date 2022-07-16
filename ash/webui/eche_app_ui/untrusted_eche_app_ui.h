// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_UNTRUSTED_ECHE_APP_UI_H_
#define ASH_WEBUI_ECHE_APP_UI_UNTRUSTED_ECHE_APP_UI_H_

#include "ui/webui/untrusted_web_ui_controller.h"
#include "ui/webui/webui_config.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {
namespace eche_app {

// WebUI config for chrome-untrusted://eche-app
class UntrustedEcheAppUIConfig : public ui::WebUIConfig {
 public:
  UntrustedEcheAppUIConfig();
  ~UntrustedEcheAppUIConfig() override;
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;
};

// WebUI controller for chrome-untrusted://eche-app
class UntrustedEcheAppUI : public ui::UntrustedWebUIController {
 public:
  explicit UntrustedEcheAppUI(content::WebUI* web_ui);
  UntrustedEcheAppUI(const UntrustedEcheAppUI&) = delete;
  UntrustedEcheAppUI& operator=(const UntrustedEcheAppUI&) = delete;
  ~UntrustedEcheAppUI() override;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_UNTRUSTED_ECHE_APP_UI_H_
