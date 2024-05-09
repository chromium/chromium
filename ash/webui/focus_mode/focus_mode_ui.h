// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FOCUS_MODE_FOCUS_MODE_UI_H_
#define ASH_WEBUI_FOCUS_MODE_FOCUS_MODE_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace ash {

// The WebUI for chrome://focus-mode-media
class FocusModeUI : public content::WebUIController {
 public:
  explicit FocusModeUI(content::WebUI* web_ui);
  ~FocusModeUI() override;
};

// The WebUIConfig for chrome://focus-mode-media
class FocusModeUIConfig : public content::WebUIConfig {
 public:
  FocusModeUIConfig();
  ~FocusModeUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace ash

#endif  // ASH_WEBUI_FOCUS_MODE_FOCUS_MODE_UI_H_
