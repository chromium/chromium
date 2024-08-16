// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_UNTRUSTED_ECHE_APP_UI_H_
#define ASH_WEBUI_ECHE_APP_UI_UNTRUSTED_ECHE_APP_UI_H_

#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {
namespace eche_app {

class UntrustedEcheAppUI;

// WebUI config for chrome-untrusted://eche-app
class UntrustedEcheAppUIConfig
    : public content::DefaultWebUIConfig<UntrustedEcheAppUI> {
 public:
  UntrustedEcheAppUIConfig();
  ~UntrustedEcheAppUIConfig() override;
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
