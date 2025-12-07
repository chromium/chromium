// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SERVICE_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace borealis {

class BorealisAppLauncher;
class BorealisAppUninstaller;
class BorealisContextManager;
class BorealisFeatures;
class BorealisInstaller;
class BorealisInstallUrlHandler;
class BorealisLaunchOptions;
class BorealisShutdownMonitor;
class BorealisWindowManager;
class BorealisSurveyHandler;

// A common location for all the interdependant components of borealis.
class BorealisService : public KeyedService {
 public:
  ~BorealisService() override = default;

  virtual BorealisAppLauncher& AppLauncher() = 0;
  virtual BorealisAppUninstaller& AppUninstaller() = 0;
  virtual BorealisContextManager& ContextManager() = 0;
  virtual BorealisFeatures& Features() = 0;
  virtual BorealisInstaller& Installer() = 0;
  virtual BorealisInstallUrlHandler& InstallUrlHandler() = 0;
  virtual BorealisLaunchOptions& LaunchOptions() = 0;
  virtual BorealisShutdownMonitor& ShutdownMonitor() = 0;
  virtual BorealisWindowManager& WindowManager() = 0;
  virtual BorealisSurveyHandler& SurveyHandler() = 0;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SERVICE_H_
