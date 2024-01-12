// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SERVICE_IMPL_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SERVICE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher_impl.h"
#include "chrome/browser/ash/borealis/borealis_app_uninstaller.h"
#include "chrome/browser/ash/borealis/borealis_context_manager_impl.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_install_url_handler.h"
#include "chrome/browser/ash/borealis/borealis_installer_impl.h"
#include "chrome/browser/ash/borealis/borealis_launch_options.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_shutdown_monitor.h"
#include "chrome/browser/ash/borealis/borealis_survey_handler.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"

namespace borealis {

class BorealisServiceImpl : public BorealisService {
 public:
  explicit BorealisServiceImpl(Profile* profile);

  ~BorealisServiceImpl() override;

 private:
  // BorealisService overrides.
  BorealisAppLauncher& AppLauncher() override;
  BorealisAppUninstaller& AppUninstaller() override;
  BorealisContextManager& ContextManager() override;
  BorealisFeatures& Features() override;
  BorealisInstaller& Installer() override;
  BorealisInstallUrlHandler& InstallUrlHandler() override;
  BorealisLaunchOptions& LaunchOptions() override;
  BorealisShutdownMonitor& ShutdownMonitor() override;
  BorealisWindowManager& WindowManager() override;
  BorealisSurveyHandler& SurveyHandler() override;

  const raw_ptr<Profile> profile_;

  BorealisAppLauncherImpl app_launcher_;
  BorealisAppUninstaller app_uninstaller_;
  BorealisContextManagerImpl context_manager_;
  BorealisFeatures features_;
  BorealisInstallerImpl installer_;
  BorealisInstallUrlHandler install_url_handler_;
  BorealisLaunchOptions launch_options_;
  BorealisShutdownMonitor shutdown_monitor_;
  BorealisWindowManager window_manager_;
  BorealisSurveyHandler survey_handler_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SERVICE_IMPL_H_
