// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_ICON_CHECKER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_ICON_CHECKER_H_

#include "chrome/browser/profiles/profile.h"
#include "components/webapps/common/web_app_id.h"

namespace ash {

// SystemWebAppIconChecker is a helper class to perform icon health checks on
// system web apps.
//
// The caller is responsible for calling StopCheck in KeyedService::Shutdown to
// avoid the underlying icon checker operating on dangling Profile pointers.
//
// This class is implemented in chrome/browser/ui/ash/system_web_app to avoid
// depending on UI code from this directory.
class SystemWebAppIconChecker {
 public:
  // Icon state reported from icon health checks.
  enum class IconState {
    // No SWA installed. We instantiate SystemWebAppManager on non-user profiles
    // without SWAs.
    // See: https://crbug.com/1353262
    kNoAppInstalled,
    // All icons are okay.
    kOk,
    // At least one the app's icon is broken.
    kBroken,
  };

  SystemWebAppIconChecker() = default;
  SystemWebAppIconChecker(const SystemWebAppIconChecker&) = delete;
  SystemWebAppIconChecker(SystemWebAppIconChecker&&) = delete;
  virtual ~SystemWebAppIconChecker() = default;

  // Creates an icon checker that verifies the currently installed system web
  // apps in `profile`.
  //
  // Implemented in chrome/browser/ui/ash/system_web_apps to avoid depending on
  // UI code here.
  static std::unique_ptr<SystemWebAppIconChecker> Create(Profile*);

  // Start a check on SWAs identified by `app_ids`. This method shouldn't be
  // called again before `callback` runs.
  virtual void StartCheck(const std::vector<webapps::AppId>& apps_ids,
                          base::OnceCallback<void(IconState)> callback) = 0;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_ICON_CHECKER_H_
