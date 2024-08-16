// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CAMERA_APP_UNTRUSTED_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CAMERA_APP_UNTRUSTED_UI_CONFIG_H_

#include "ash/webui/camera_app_ui/camera_app_untrusted_ui.h"
#include "content/public/browser/webui_config.h"

namespace ash {

class CameraAppUntrustedUIConfig
    : public content::DefaultWebUIConfig<CameraAppUntrustedUI> {
 public:
  CameraAppUntrustedUIConfig();
  CameraAppUntrustedUIConfig(const CameraAppUntrustedUIConfig& other) = delete;
  CameraAppUntrustedUIConfig& operator=(const CameraAppUntrustedUIConfig&) =
      delete;
  ~CameraAppUntrustedUIConfig() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CAMERA_APP_UNTRUSTED_UI_CONFIG_H_
