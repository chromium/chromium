// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_APP_SETUP_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_APP_SETUP_CONTROLLER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

namespace ash {
namespace android_sms {

// Manages the setup and uninstallation process of the Android SMS PWA.
class AndroidSmsAppSetupController {
 public:
  AndroidSmsAppSetupController() = default;

  AndroidSmsAppSetupController(const AndroidSmsAppSetupController&) = delete;
  AndroidSmsAppSetupController& operator=(const AndroidSmsAppSetupController&) =
      delete;

  virtual ~AndroidSmsAppSetupController() = default;

  using SuccessCallback = base::OnceCallback<void(bool)>;

  // Performs the setup process for the app at |app_url|, which includes:
  //   (1) Installing the PWA,
  //   (2) Granting permission for the PWA to show notifications, and
  //   (3) Removing previously set migration cookie, if any.
  //   (4) Setting a cookie which defaults the PWA to remember this computer.
  // The |app_url|  parameter should have the root URL of the app to install
  // and should be the same as the service worker scope
  // The |install_url| parameter is the url to install the app from and cannot
  // redirect.
  virtual void SetUpApp(const GURL& app_url,
                        const GURL& install_url,
                        SuccessCallback callback) = 0;

  // Returns the id for the PWA at |install_url|; if no PWA exists,
  // std::nullopt is returned.
  virtual std::optional<webapps::AppId> GetPwa(const GURL& install_url) = 0;

  // Deletes the cookie which causes the PWA to remember this computer by
  // default. Note that this does not actually stop the PWA from remembering
  // this computer; rather, it stops the PWA from *defaulting* to remember the
  // computer in the case that the user has not gone through the PWA's setup.
  virtual void DeleteRememberDeviceByDefaultCookie(
      const GURL& app_url,
      SuccessCallback callback) = 0;

  // Uninstalls the app at |app_url| and deletes relevant cookies from the setup
  // process. This also sets a migration cookie that allows the client to start
  // redirecting users to the new domain.
  // The |app_url| parameter should have the root URL of the app to install
  // and should be the same as the service worker scope
  // The |install_url| parameter is the url to that was used to install the app.
  // The |migrated_to_app_url| should be the url that the PWA was migrated to.
  virtual void RemoveApp(const GURL& app_url,
                         const GURL& install_url,
                         const GURL& migrated_to_app_url,
                         SuccessCallback callback) = 0;
};

}  // namespace android_sms
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_APP_SETUP_CONTROLLER_H_
