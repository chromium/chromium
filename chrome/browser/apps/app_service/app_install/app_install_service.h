// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_

#include <iosfwd>

#include "base/functional/callback_forward.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"

class Profile;

namespace apps {

class PackageId;

// Service for requesting installation of apps on ChromeOS.
//
// New users of these APIs should add a new AppInstallSurface entry, which is
// used to make decisions on behavior and record metrics per-usecase.
class AppInstallService {
 public:
  static std::unique_ptr<AppInstallService> Create(Profile& profile);

  virtual ~AppInstallService();

  // Requests installation of the app with ID `package_id` from `surface`. This
  // communicates with the Almanac app API to retrieve app data, and then
  // prompts the user to proceed with the installation. `callback` is called
  // when the installation completes, whether successful or unsuccessful.
  // If an app with the same package_id is already installed that app will be
  // launched instead.
  virtual void InstallApp(AppInstallSurface surface,
                          PackageId package_id,
                          base::OnceClosure callback) = 0;

// Not needed by Lacros clients, so can avoid adding to the crosapi.
#if BUILDFLAG(IS_CHROMEOS_ASH)

  // Requests installation of the app with ID `package_id` from `surface`. This
  // communicates with the Almanac app API to retrieve app data, and then
  // silently installs the app without further prompting. `callback` is called
  // when the installation completes, whether successful or unsuccessful.
  virtual void InstallAppHeadless(
      AppInstallSurface surface,
      PackageId package_id,
      base::OnceCallback<void(bool success)> callback) = 0;

  // Requests installation of the app `data` from `surface`. This silently
  // installs the given data with no further prompting. `callback` is called
  // when the installation completes, whether successful or unsuccessful.
  virtual void InstallAppHeadless(
      AppInstallSurface surface,
      AppInstallData data,
      base::OnceCallback<void(bool success)> callback) = 0;
#endif
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_
