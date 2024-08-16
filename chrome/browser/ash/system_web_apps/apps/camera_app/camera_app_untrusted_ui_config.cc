// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/camera_app/camera_app_untrusted_ui_config.h"

#include <memory>

#include "ash/webui/camera_app_ui/url_constants.h"
#include "content/public/common/url_constants.h"

namespace ash {

CameraAppUntrustedUIConfig::CameraAppUntrustedUIConfig()
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme,
                         kChromeUICameraAppHost) {}

CameraAppUntrustedUIConfig::~CameraAppUntrustedUIConfig() = default;

}  // namespace ash
