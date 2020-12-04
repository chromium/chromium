// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_service_fake.h"

#include "chrome/browser/chromeos/borealis/borealis_service_factory.h"

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

BorealisServiceFake::~BorealisServiceFake() = default;

BorealisAppLauncher& BorealisServiceFake::AppLauncher() {
  DCHECK(app_launcher_);
  return *app_launcher_;
}

BorealisContextManager& BorealisServiceFake::ContextManager() {
  DCHECK(context_manager_);
  return *context_manager_;
}

BorealisFeatures& BorealisServiceFake::Features() {
  DCHECK(features_);
  return *features_;
}

BorealisInstaller& BorealisServiceFake::Installer() {
  DCHECK(installer_);
  return *installer_;
}

BorealisShutdownMonitor& BorealisServiceFake::ShutdownMonitor() {
  DCHECK(shutdown_monitor_);
  return *shutdown_monitor_;
}

BorealisWindowManager& BorealisServiceFake::WindowManager() {
  DCHECK(window_manager_);
  return *window_manager_;
}

void BorealisServiceFake::SetAppLauncherForTesting(
    BorealisAppLauncher* app_launcher) {
  app_launcher_ = app_launcher;
}

void BorealisServiceFake::SetContextManagerForTesting(
    BorealisContextManager* context_manager) {
  context_manager_ = context_manager;
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
