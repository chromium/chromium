// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_browser_controller_ash.h"

namespace ash {

WebKioskBrowserControllerAsh::WebKioskBrowserControllerAsh(
    web_app::WebAppProvider& provider,
    Browser* browser,
    webapps::AppId app_id,
    const ash::SystemWebAppDelegate* system_app)
    : WebKioskBrowserControllerBase(provider, browser, app_id),
      system_app_(system_app) {}

WebKioskBrowserControllerAsh::~WebKioskBrowserControllerAsh() = default;

const ash::SystemWebAppDelegate* WebKioskBrowserControllerAsh::system_app()
    const {
  return system_app_;
}

}  // namespace ash
