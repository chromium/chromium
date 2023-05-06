// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MULTIDEVICE_DEBUG_PROXIMITY_AUTH_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_MULTIDEVICE_DEBUG_PROXIMITY_AUTH_UI_CONFIG_H_

#include "ash/webui/multidevice_debug/url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace content {
class BrowserContext;
class WebUI;
}  // namespace content

namespace ash::multidevice {

// If possible, prefer defining WebUIConfigs under //ash alongside its
// corresponding WebUIController.
// ProximityAuthUIConfig needs to live under //chrome as Profile is needed by
// both `IsWebUIEnabled` and `CreateWebUIController`.
//
// The WebUIConfig for chrome://proximity-auth.
class ProximityAuthUIConfig : public content::WebUIConfig {
 public:
  ProximityAuthUIConfig();
  ~ProximityAuthUIConfig() override = default;

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

}  // namespace ash::multidevice

#endif  // CHROME_BROWSER_ASH_MULTIDEVICE_DEBUG_PROXIMITY_AUTH_UI_CONFIG_H_
