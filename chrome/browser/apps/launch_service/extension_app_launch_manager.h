// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LAUNCH_SERVICE_EXTENSION_APP_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_APPS_LAUNCH_SERVICE_EXTENSION_APP_LAUNCH_MANAGER_H_

#include "base/macros.h"
#include "chrome/browser/apps/launch_service/launch_manager.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace apps {

// Handles launch requests for extension-backed apps,
// including Chrome Apps (platform apps and legacy packaged apps) and hosted
// apps (including desktop PWAs until BMO is ready).
class ExtensionAppLaunchManager final : public LaunchManager {
 public:
  explicit ExtensionAppLaunchManager(Profile* profile);
  ~ExtensionAppLaunchManager() override;

  // apps::LaunchManager:
  content::WebContents* OpenApplication(const AppLaunchParams& params) override;

  bool OpenApplicationWindow(const std::string& app_id,
                             const base::CommandLine& command_line,
                             const base::FilePath& current_directory) override;

  bool OpenApplicationTab(const std::string& app_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionAppLaunchManager);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LAUNCH_SERVICE_EXTENSION_APP_LAUNCH_MANAGER_H_
