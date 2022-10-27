// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_test_helper.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_paths.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"

namespace ash {

DemoModeTestHelper::DemoModeTestHelper()
    : browser_process_platform_part_test_api_(
          TestingBrowserProcess::GetGlobal()->platform_part()) {
  if (!ConciergeClient::Get()) {
    concierge_client_initialized_ = true;
    ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
  }

  DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kNone);

  CHECK(components_temp_dir_.CreateUniqueTempDir());
  components_path_override_ = std::make_unique<base::ScopedPathOverride>(
      DIR_PREINSTALLED_COMPONENTS, components_temp_dir_.GetPath());

  CHECK(base::CreateDirectory(GetDemoResourcesPath()));
  CHECK(base::CreateDirectory(GetPreinstalledDemoResourcesPath()));
}

DemoModeTestHelper::~DemoModeTestHelper() {
  if (concierge_client_initialized_) {
    ConciergeClient::Shutdown();
  }
  DemoSession::ShutDownIfInitialized();
  DemoSession::ResetDemoConfigForTesting();
  if (fake_cros_component_manager_) {
    fake_cros_component_manager_ = nullptr;
    browser_process_platform_part_test_api_.ShutdownCrosComponentManager();
  }
}

void DemoModeTestHelper::InitializeSession(DemoSession::DemoModeConfig config) {
  DCHECK_NE(config, DemoSession::DemoModeConfig::kNone);
  DemoSession::SetDemoConfigForTesting(config);

  InitializeCrosComponentManager();
  CHECK(DemoSession::StartIfInDemoMode());
  FinishLoadingComponent();
}

void DemoModeTestHelper::InitializeSessionWithPendingComponent(
    DemoSession::DemoModeConfig config) {
  DCHECK_NE(config, DemoSession::DemoModeConfig::kNone);
  DemoSession::SetDemoConfigForTesting(config);
  InitializeCrosComponentManager();

  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  DCHECK_EQ(demo_session == nullptr,
            config == DemoSession::DemoModeConfig::kNone);
}

base::FilePath DemoModeTestHelper::GetDemoResourcesPath() {
  return components_temp_dir_.GetPath()
      .AppendASCII("cros-components")
      .AppendASCII(DemoComponents::kDemoModeResourcesComponentName);
}

base::FilePath DemoModeTestHelper::GetPreinstalledDemoResourcesPath() {
  return components_temp_dir_.GetPath()
      .AppendASCII("cros-components")
      .AppendASCII(DemoComponents::kOfflineDemoModeResourcesComponentName);
}

void DemoModeTestHelper::InitializeCrosComponentManager() {
  auto cros_component_manager =
      base::MakeRefCounted<component_updater::FakeCrOSComponentManager>();
  fake_cros_component_manager_ = cros_component_manager.get();

  // Set up the Demo Mode Resources component. Ensure we queue load requests
  // so components don't load instantly.
  cros_component_manager->set_queue_load_requests(true);
  cros_component_manager->set_supported_components(
      {DemoComponents::kDemoModeResourcesComponentName});

  browser_process_platform_part_test_api_.InitializeCrosComponentManager(
      std::move(cros_component_manager));
}

void DemoModeTestHelper::FinishLoadingComponent() {
  base::RunLoop run_loop;
  DemoSession::Get()->EnsureResourcesLoaded(run_loop.QuitClosure());

  // TODO(michaelpg): Update once offline Demo Mode also uses a CrOS component.
  if (DemoSession::GetDemoConfig() == DemoSession::DemoModeConfig::kOnline) {
    CHECK(fake_cros_component_manager_->FinishLoadRequest(
        DemoComponents::kDemoModeResourcesComponentName,
        component_updater::FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::NONE,
            base::FilePath("/dev/null"), GetDemoResourcesPath())));
  } else {
    CHECK(!fake_cros_component_manager_->HasPendingInstall(
        DemoComponents::kDemoModeResourcesComponentName));
  }

  run_loop.Run();
}

void DemoModeTestHelper::FailLoadingComponent() {
  base::RunLoop run_loop;
  DemoSession::Get()->EnsureResourcesLoaded(run_loop.QuitClosure());

  // TODO(michaelpg): Update once offline Demo Mode also uses a CrOS component.
  if (DemoSession::GetDemoConfig() == DemoSession::DemoModeConfig::kOnline) {
    CHECK(fake_cros_component_manager_->FinishLoadRequest(
        DemoComponents::kDemoModeResourcesComponentName,
        component_updater::FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::INSTALL_FAILURE,
            base::FilePath(), base::FilePath())));
  }
  run_loop.Run();
}

}  // namespace ash
