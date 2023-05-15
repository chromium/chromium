// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_APP_TRUSTED_PROJECTOR_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_APP_TRUSTED_PROJECTOR_UI_CONFIG_H_

#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "content/public/common/url_constants.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class TrustedProjectorUI;

// If possible, prefer defining WebUIConfigs under //ash alongside its
// corresponding WebUIController.
// TrustedProjectorUIConfig needs to live under //chrome as Profile is needed by
// both `IsWebUIEnabled` and `CreateWebUIController`.
//
// The WebUIConfig of the Projector player app WebUI.
class TrustedProjectorUIConfig
    : public SystemWebAppUIConfig<TrustedProjectorUI> {
 public:
  TrustedProjectorUIConfig();

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_APP_TRUSTED_PROJECTOR_UI_CONFIG_H_
