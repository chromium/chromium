// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_BROWSER_CONTROLLER_ASH_H_
#define CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_BROWSER_CONTROLLER_ASH_H_

#include "chrome/browser/chromeos/app_mode/web_kiosk_browser_controller_base.h"

#include "components/webapps/common/web_app_id.h"

namespace ash {

class SystemWebAppDelegate;

// Applies web Kiosk restrictions to the browser in Ash.
class WebKioskBrowserControllerAsh
    : public chromeos::WebKioskBrowserControllerBase {
 public:
  WebKioskBrowserControllerAsh(web_app::WebAppProvider& provider,
                               Browser* browser,
                               webapps::AppId app_id,
                               const ash::SystemWebAppDelegate* system_app);
  WebKioskBrowserControllerAsh(const WebKioskBrowserControllerAsh&) = delete;
  WebKioskBrowserControllerAsh& operator=(const WebKioskBrowserControllerAsh&) =
      delete;
  ~WebKioskBrowserControllerAsh() override;

  const ash::SystemWebAppDelegate* system_app() const override;

 private:
  // The system app (if any) associated with the WebContents we're in.
  raw_ptr<const ash::SystemWebAppDelegate> system_app_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_BROWSER_CONTROLLER_ASH_H_
