// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_

#include <iosfwd>

#include "base/functional/callback_forward.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"

namespace apps {

class PackageId;

class AppInstallService {
 public:
  virtual ~AppInstallService();

  virtual void InstallApp(AppInstallSurface surface,
                          PackageId package_id,
                          base::OnceClosure callback) = 0;

// Not needed by Lacros clients so can avoid adding to the crosapi.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  virtual void InstallApp(AppInstallSurface surface,
                          AppInstallData data,
                          base::OnceCallback<void(bool success)> callback) = 0;
#endif
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_
