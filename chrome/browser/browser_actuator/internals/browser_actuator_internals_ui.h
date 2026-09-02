// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_UI_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_UI_H_

#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

inline constexpr char kChromeUIBrowserActuatorInternalsHost[] =
    "browser-actuator-internals";

// WebUIController for chrome://browser-actuator-internals/.
class BrowserActuatorInternalsUI : public content::WebUIController {
 public:
  explicit BrowserActuatorInternalsUI(content::WebUI* web_ui);
  BrowserActuatorInternalsUI(const BrowserActuatorInternalsUI&) = delete;
  BrowserActuatorInternalsUI& operator=(const BrowserActuatorInternalsUI&) =
      delete;
  ~BrowserActuatorInternalsUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

class BrowserActuatorInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<BrowserActuatorInternalsUI> {
 public:
  BrowserActuatorInternalsUIConfig();
  ~BrowserActuatorInternalsUIConfig() override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_UI_H_
