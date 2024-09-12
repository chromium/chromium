// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_APP_INSTALL_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_APP_INSTALL_UTIL_H_

#include <optional>
#include <tuple>

#include "base/check_deref.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

class Profile;

namespace chromeos {
// Convenience alias for the result of `GetWebKioskAppInstallState`.
//
// The `AppId` is only present when `WebKioskInstallState` is `kInstalled`.
// Otherwise it is `std::nullopt`.
using KioskWebAppInstallState = std::tuple<crosapi::mojom::WebKioskInstallState,
                                           std::optional<webapps::AppId>>;

// Returns the install state of a web app given by its `install_url` in the
// given `profile`.
KioskWebAppInstallState GetKioskWebAppInstallState(Profile& profile,
                                                   const GURL& install_url);

// Installs the web app given by `install_url` in the given `profile`.
void InstallKioskWebApp(
    Profile& profile,
    const GURL& install_url,
    crosapi::mojom::WebKioskInstaller::InstallWebKioskCallback on_done);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_APP_INSTALL_UTIL_H_
