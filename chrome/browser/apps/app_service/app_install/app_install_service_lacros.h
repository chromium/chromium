// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_LACROS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_LACROS_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_install/app_install_service.h"

namespace apps {

class AppInstallServiceLacros : public AppInstallService {
 public:
  AppInstallServiceLacros();
  ~AppInstallServiceLacros() override;

  // AppInstallService:
  void InstallAppWithFallback(
      AppInstallSurface surface,
      std::string serialized_package_id,
      std::optional<base::UnguessableToken> anchor_window,
      base::OnceClosure callback) override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_SERVICE_LACROS_H_
