// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_CONFIG_H_

#include <memory>

#include "ash/webui/projector_app/untrusted_projector_ui.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"

class GURL;

namespace content {
class WebUIDataSource;
class WebUI;
class WebUIController;
}  // namespace content

// Implementation of the chromeos::UntrustedProjectorUIDelegate to expose some
// //chrome functions to //chromeos.
class ChromeUntrustedProjectorUIDelegate
    : public ash::UntrustedProjectorUIDelegate {
 public:
  ChromeUntrustedProjectorUIDelegate();
  ChromeUntrustedProjectorUIDelegate(
      const ChromeUntrustedProjectorUIDelegate&) = delete;
  ChromeUntrustedProjectorUIDelegate& operator=(
      const ChromeUntrustedProjectorUIDelegate&) = delete;

  // ash::UntrustedProjectorUIDelegate:
  void PopulateLoadTimeData(content::WebUIDataSource* source) override;
};

// A webui config for the chrome-untrusted:// part of Projector.
class UntrustedProjectorUIConfig
    : public ash::SystemWebAppUntrustedUIConfig<ash::UntrustedProjectorUI> {
 public:
  UntrustedProjectorUIConfig();
  UntrustedProjectorUIConfig(const UntrustedProjectorUIConfig& other) = delete;
  UntrustedProjectorUIConfig& operator=(const UntrustedProjectorUIConfig&) =
      delete;
  ~UntrustedProjectorUIConfig() override;

  // ash::SystemWebAppUntrustedUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_CONFIG_H_
