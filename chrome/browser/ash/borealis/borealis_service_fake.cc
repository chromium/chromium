// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_service_fake.h"

#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"

namespace borealis {

// static
BorealisServiceFake* BorealisServiceFake::UseFakeForTesting(
    content::BrowserContext* context) {
  return static_cast<BorealisServiceFake*>(
      borealis::BorealisServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          context, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
            return std::make_unique<BorealisServiceFake>();
          })));
}

BorealisServiceFake::BorealisServiceFake() = default;
BorealisServiceFake::~BorealisServiceFake() = default;

BorealisAppLauncher& BorealisServiceFake::AppLauncher() {
  CHECK(app_launcher_);
  return *app_launcher_;
}

BorealisAppUninstaller& BorealisServiceFake::AppUninstaller() {
  CHECK(app_uninstaller_);
  return *app_uninstaller_;
}

BorealisContextManager& BorealisServiceFake::ContextManager() {
  CHECK(context_manager_);
  return *context_manager_;
}

BorealisDiskManagerDispatcher& BorealisServiceFake::DiskManagerDispatcher() {
  CHECK(borealis_disk_manager_dispatcher_);
  return *borealis_disk_manager_dispatcher_;
}

BorealisFeatures& BorealisServiceFake::Features() {
  CHECK(features_);
  return *features_;
}

BorealisInstaller& BorealisServiceFake::Installer() {
  CHECK(installer_);
  return *installer_;
}

BorealisLaunchOptions& BorealisServiceFake::LaunchOptions() {
  CHECK(launch_options_);
  return *launch_options_;
}

BorealisShutdownMonitor& BorealisServiceFake::ShutdownMonitor() {
  CHECK(shutdown_monitor_);
  return *shutdown_monitor_;
}

BorealisWindowManager& BorealisServiceFake::WindowManager() {
  CHECK(window_manager_);
  return *window_manager_;
}

void BorealisServiceFake::SetAppLauncherForTesting(
    BorealisAppLauncher* app_launcher) {
  app_launcher_ = app_launcher;
}

void BorealisServiceFake::SetAppUninstallerForTesting(
    BorealisAppUninstaller* app_uninstaller) {
  app_uninstaller_ = app_uninstaller;
}

void BorealisServiceFake::SetContextManagerForTesting(
    BorealisContextManager* context_manager) {
  context_manager_ = context_manager;
}

void BorealisServiceFake::SetDiskManagerDispatcherForTesting(
    BorealisDiskManagerDispatcher* borealis_disk_manager_dispatcher) {
  borealis_disk_manager_dispatcher_ = borealis_disk_manager_dispatcher;
}

void BorealisServiceFake::SetFeaturesForTesting(BorealisFeatures* features) {
  features_ = features;
}

void BorealisServiceFake::SetInstallerForTesting(BorealisInstaller* installer) {
  installer_ = installer;
}

void BorealisServiceFake::SetShutdownMonitorForTesting(
    BorealisShutdownMonitor* shutdown_monitor) {
  shutdown_monitor_ = shutdown_monitor;
}

void BorealisServiceFake::SetWindowManagerForTesting(
    BorealisWindowManager* window_manager) {
  window_manager_ = window_manager;
}

}  // namespace borealis
