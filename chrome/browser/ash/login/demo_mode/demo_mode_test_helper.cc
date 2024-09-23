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
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"

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

  CHECK(base::CreateDirectory(GetDemoResourcesPath()));
}

DemoModeTestHelper::~DemoModeTestHelper() {
  if (concierge_client_initialized_) {
    ConciergeClient::Shutdown();
  }

  DemoSession::ShutDownIfInitialized();

  DemoSession::ResetDemoConfigForTesting();
  if (fake_component_manager_ash_) {
    fake_component_manager_ash_ = nullptr;
    browser_process_platform_part_test_api_.ShutdownComponentManager();
  }
}

void DemoModeTestHelper::InitializeSession(DemoSession::DemoModeConfig config) {
  DCHECK_NE(config, DemoSession::DemoModeConfig::kNone);
  DemoSession::SetDemoConfigForTesting(config);

  InitializeComponentManager();
  CHECK(DemoSession::StartIfInDemoMode());
  FinishLoadingComponent();
}

void DemoModeTestHelper::InitializeSessionWithPendingComponent(
    DemoSession::DemoModeConfig config) {
  DCHECK_NE(config, DemoSession::DemoModeConfig::kNone);
  DemoSession::SetDemoConfigForTesting(config);
  InitializeComponentManager();

  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  DCHECK_EQ(demo_session == nullptr,
            config == DemoSession::DemoModeConfig::kNone);
}

base::FilePath DemoModeTestHelper::GetDemoResourcesPath() {
  return components_temp_dir_.GetPath()
      .AppendASCII("cros-components")
      .AppendASCII(DemoComponents::kDemoModeResourcesComponentName);
}

void DemoModeTestHelper::InitializeComponentManager() {
  auto component_manager_ash =
      base::MakeRefCounted<component_updater::FakeComponentManagerAsh>();
  fake_component_manager_ash_ = component_manager_ash.get();

  // Set up the Demo Mode Resources component. Ensure we queue load requests
  // so components don't load instantly.
  component_manager_ash->set_queue_load_requests(true);
  component_manager_ash->set_supported_components(
      {DemoComponents::kDemoModeResourcesComponentName});

  browser_process_platform_part_test_api_.InitializeComponentManager(
      std::move(component_manager_ash));
}

void DemoModeTestHelper::FinishLoadingComponent() {
  base::RunLoop run_loop;
  DemoSession::Get()->EnsureResourcesLoaded(run_loop.QuitClosure());

  // TODO(michaelpg): Update once offline Demo Mode also uses a CrOS component.
  if (DemoSession::GetDemoConfig() == DemoSession::DemoModeConfig::kOnline) {
    CHECK(fake_component_manager_ash_->FinishLoadRequest(
        DemoComponents::kDemoModeResourcesComponentName,
        component_updater::FakeComponentManagerAsh::ComponentInfo(
            component_updater::ComponentManagerAsh::Error::NONE,
            base::FilePath("/dev/null"), GetDemoResourcesPath())));
  } else {
    CHECK(!fake_component_manager_ash_->HasPendingInstall(
        DemoComponents::kDemoModeResourcesComponentName));
  }

  run_loop.Run();
}

void DemoModeTestHelper::FailLoadingComponent() {
  base::RunLoop run_loop;
  DemoSession::Get()->EnsureResourcesLoaded(run_loop.QuitClosure());

  // TODO(michaelpg): Update once offline Demo Mode also uses a CrOS component.
  if (DemoSession::GetDemoConfig() == DemoSession::DemoModeConfig::kOnline) {
    CHECK(fake_component_manager_ash_->FinishLoadRequest(
        DemoComponents::kDemoModeResourcesComponentName,
        component_updater::FakeComponentManagerAsh::ComponentInfo(
            component_updater::ComponentManagerAsh::Error::INSTALL_FAILURE,
            base::FilePath(), base::FilePath())));
  }
  run_loop.Run();
}

}  // namespace ash
