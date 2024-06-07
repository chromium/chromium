// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FOCUS_MODE_FOCUS_MODE_UNTRUSTED_UI_H_
#define ASH_WEBUI_FOCUS_MODE_FOCUS_MODE_UNTRUSTED_UI_H_

#include <memory>

#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ash {

// The WebUI for chrome-untrusted://focus-mode-player.
class FocusModeUntrustedUI : public ui::UntrustedWebUIController {
 public:
  explicit FocusModeUntrustedUI(content::WebUI* web_ui);
  ~FocusModeUntrustedUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

// The WebUIConfig for chrome-untrusted://focus-mode-player.
class FocusModeUntrustedUIConfig : public content::WebUIConfig {
 public:
  FocusModeUntrustedUIConfig();

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace ash

#endif  // ASH_WEBUI_FOCUS_MODE_FOCUS_MODE_UNTRUSTED_UI_H_
