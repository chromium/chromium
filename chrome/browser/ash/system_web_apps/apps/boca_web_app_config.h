// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_BOCA_WEB_APP_CONFIG_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_BOCA_WEB_APP_CONFIG_H_

#include "ash/webui/boca_ui/boca_ui.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace ash {
// WebUI config for Boca SWA.
class BocaUIConfig : public content::DefaultWebUIConfig<ash::boca::BocaUI> {
 public:
  BocaUIConfig();

  // content::DefaultWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_BOCA_WEB_APP_CONFIG_H_
