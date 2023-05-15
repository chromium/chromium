// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/camera_app/camera_app_untrusted_ui_config.h"

#include <memory>

#include "ash/webui/camera_app_ui/camera_app_untrusted_ui.h"
#include "ash/webui/camera_app_ui/url_constants.h"
#include "content/public/common/url_constants.h"

namespace ash {

CameraAppUntrustedUIConfig::CameraAppUntrustedUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme, kChromeUICameraAppHost) {}

CameraAppUntrustedUIConfig::~CameraAppUntrustedUIConfig() = default;

std::unique_ptr<content::WebUIController>
CameraAppUntrustedUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                  const GURL& url) {
  return std::make_unique<CameraAppUntrustedUI>(web_ui);
}

}  // namespace ash
