// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_HELP_APP_HELP_APP_UNTRUSTED_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_HELP_APP_HELP_APP_UNTRUSTED_UI_CONFIG_H_

#include "content/public/browser/webui_config.h"

namespace ash {

class HelpAppUntrustedUIConfig : public content::WebUIConfig {
 public:
  HelpAppUntrustedUIConfig();
  HelpAppUntrustedUIConfig(const HelpAppUntrustedUIConfig& other) = delete;
  HelpAppUntrustedUIConfig& operator=(const HelpAppUntrustedUIConfig&) = delete;
  ~HelpAppUntrustedUIConfig() override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_HELP_APP_HELP_APP_UNTRUSTED_UI_CONFIG_H_
