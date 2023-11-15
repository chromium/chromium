// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_

#include <iosfwd>

#include "base/functional/callback_forward.h"

namespace apps {

class PackageId;

enum class AppInstallSurface {
  kAppInstallNavigationThrottle,
};

std::ostream& operator<<(std::ostream& out, AppInstallSurface surface);

class AppInstallService {
 public:
  virtual ~AppInstallService();

  virtual void InstallApp(AppInstallSurface surface,
                          PackageId package_id,
                          base::OnceClosure callback) = 0;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_H_
