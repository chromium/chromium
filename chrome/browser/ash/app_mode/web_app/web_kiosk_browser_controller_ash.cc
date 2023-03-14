// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_browser_controller_ash.h"

namespace ash {

WebKioskBrowserControllerAsh::WebKioskBrowserControllerAsh(
    web_app::WebAppProvider& provider,
    Browser* browser,
    web_app::AppId app_id)
    : WebKioskBrowserControllerBase(provider, browser, app_id) {}

WebKioskBrowserControllerAsh::~WebKioskBrowserControllerAsh() = default;

}  // namespace ash
