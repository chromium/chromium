// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_ARC_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_ARC_TEST_HELPER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"

class ArcAppListPrefs;
class Profile;

namespace arc {
class FakeAppInstance;
namespace mojom {
class AppHost;
}
}  // namespace arc

namespace base {
class CommandLine;
}

namespace ash {

// A helper class that does the required setup for running tests with a fake ARC
// instance.
class AppRestoreArcTestHelper {
 public:
  AppRestoreArcTestHelper();
  AppRestoreArcTestHelper(const AppRestoreArcTestHelper&) = delete;
  AppRestoreArcTestHelper& operator=(const AppRestoreArcTestHelper&) = delete;
  ~AppRestoreArcTestHelper();

  // Owners should call these at the various browser test setup phases.
  void SetUpCommandLine(base::CommandLine* command_line);
  void SetUpInProcessBrowserTestFixture();
  void SetUpOnMainThread(Profile* profile);

  void StartInstance();
  void StopInstance();

  void SendPackageAdded(const std::string& package_name);
  void InstallTestApps(const std::string& package_name, bool multi_app);
  void CreateTask(const std::string& app_id,
                  int32_t task_id,
                  int32_t session_id);
  void UpdateThemeColor(int32_t task_id,
                        uint32_t primary_color,
                        uint32_t status_bar_color);

  ArcAppListPrefs* GetAppPrefs();
  arc::mojom::AppHost* GetAppHost();

 private:
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;

  std::unique_ptr<arc::FakeAppInstance> app_instance_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_ARC_TEST_HELPER_H_
