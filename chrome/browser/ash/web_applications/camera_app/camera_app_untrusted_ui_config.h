// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_CAMERA_APP_CAMERA_APP_UNTRUSTED_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_CAMERA_APP_CAMERA_APP_UNTRUSTED_UI_CONFIG_H_

#include "content/public/browser/webui_config.h"

namespace ash {

class CameraAppUntrustedUIConfig : public content::WebUIConfig {
 public:
  CameraAppUntrustedUIConfig();
  CameraAppUntrustedUIConfig(const CameraAppUntrustedUIConfig& other) = delete;
  CameraAppUntrustedUIConfig& operator=(const CameraAppUntrustedUIConfig&) =
      delete;
  ~CameraAppUntrustedUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_CAMERA_APP_CAMERA_APP_UNTRUSTED_UI_CONFIG_H_
