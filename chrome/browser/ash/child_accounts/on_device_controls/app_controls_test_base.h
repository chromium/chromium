// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_TEST_BASE_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_TEST_BASE_H_

#include <string>

#include "base/test/scoped_command_line.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/browser_task_environment.h"

namespace ash::on_device_controls {

// Base unit test class for testing on device app controls.
class AppControlsTestBase : public ChromeViewsTestBase {
 public:
  AppControlsTestBase();
  AppControlsTestBase(const AppControlsTestBase&) = delete;
  AppControlsTestBase& operator=(const AppControlsTestBase&) = delete;
  ~AppControlsTestBase() override;

  ArcAppTest& arc_test() { return arc_test_; }
  apps::AppServiceTest& app_service_test() { return app_service_test_; }
  Profile& profile() { return profile_; }

  // Installs ARC++ app with the given `package_name` and `app_name`.
  // Returns AppService id of the installed app.
  std::string InstallArcApp(const std::string& package_name,
                            const std::string& app_name);
  // Uninstalls ARC++ app with the given `package_name`.
  void UninstallArcApp(const std::string& package_name);

  // ChromeViewsTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  base::test::ScopedCommandLine scoped_command_line_;

  TestingProfile profile_;
  apps::AppServiceTest app_service_test_;
  ArcAppTest arc_test_;
};

}  // namespace ash::on_device_controls

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_TEST_BASE_H_
