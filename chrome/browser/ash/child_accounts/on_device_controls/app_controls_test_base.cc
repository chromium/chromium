// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_test_base.h"

#include <string>
#include <vector>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/command_line.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/child_accounts/apps/app_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_task_environment.h"

namespace ash::on_device_controls {

AppControlsTestBase::AppControlsTestBase() = default;

AppControlsTestBase::~AppControlsTestBase() = default;

void AppControlsTestBase::SetUp() {
  testing::Test::SetUp();

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDefaultApps);

  app_service_test_.SetUp(&profile_);
  arc_test_.SetUp(&profile_);
  task_environment_.RunUntilIdle();
}

void AppControlsTestBase::TearDown() {
  arc_test_.TearDown();

  testing::Test::TearDown();
}

std::string AppControlsTestBase::InstallArcApp(const std::string& package_name,
                                               const std::string& app_name) {
  arc_test_.AddPackage(CreateArcAppPackage(package_name)->Clone());
  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.emplace_back(CreateArcAppInfo(package_name, app_name));
  arc_test_.app_instance()->SendPackageAppListRefreshed(package_name, apps);
  task_environment_.RunUntilIdle();

  return arc::ArcPackageNameToAppId(package_name, &profile_);
}

void AppControlsTestBase::UninstallArcApp(const std::string& package_name) {
  arc_test_.app_instance()->UninstallPackage(package_name);
  task_environment_.RunUntilIdle();
}

}  // namespace ash::on_device_controls
