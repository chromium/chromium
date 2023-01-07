// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_TEST_HELPER_H_

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"

namespace base {
class ScopedPathOverride;
}  // namespace base

namespace component_updater {
class FakeCrOSComponentManager;
}  // namespace component_updater

namespace ash {

// Creating a DemoModeTestHelper doesn't enable Demo Mode; it just sets up an
// environment that can support Demo Mode in tests.
// To start a Demo Mode session, use InitializeSession().
class DemoModeTestHelper {
 public:
  DemoModeTestHelper();

  DemoModeTestHelper(const DemoModeTestHelper&) = delete;
  DemoModeTestHelper& operator=(const DemoModeTestHelper&) = delete;

  ~DemoModeTestHelper();

  // Starts a Demo Mode session and loads a fake Demo Mode resources component.
  void InitializeSession(DemoSession::DemoModeConfig config =
                             DemoSession::DemoModeConfig::kOnline);

  // Starts a Demo Mode session but does not finish loading a fake Demo Mode
  // resources component. Used to test set-up flows and sessions with no
  // resources.
  void InitializeSessionWithPendingComponent(
      DemoSession::DemoModeConfig config =
          DemoSession::DemoModeConfig::kOnline);

  // Manually succeeds or fails to load the Demo Mode resources, then waits
  // for all DemoSession::EnsureOfflineResourcesLoaded callbacks to be called
  // before returning.
  // Only use with InitializeSessionWithPendingComponent().
  void FinishLoadingComponent();
  void FailLoadingComponent();

  // Returns the path that fake Demo Mode resources will be mounted from.
  base::FilePath GetDemoResourcesPath();

  // Returns the path that fake offline Demo Mode resources will be preinstalled
  // at.
  base::FilePath GetPreinstalledDemoResourcesPath();

 private:
  void InitializeCrosComponentManager();

  BrowserProcessPlatformPartTestApi browser_process_platform_part_test_api_;

  base::ScopedTempDir components_temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> components_path_override_;

  // Raw ponter to the FakeCrOSComponentManager once created.
  component_updater::FakeCrOSComponentManager* fake_cros_component_manager_ =
      nullptr;

  // True if this class initialized the ConciergeClient.
  bool concierge_client_initialized_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_TEST_HELPER_H_
