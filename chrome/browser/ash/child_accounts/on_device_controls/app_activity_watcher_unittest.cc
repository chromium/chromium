// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/app_activity_watcher.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_test_base.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_registry.h"
#include "chrome/browser/ui/views/apps/app_dialog/app_local_block_dialog_view.h"
#include "ui/aura/window.h"

namespace ash::on_device_controls {

class AppActivityWatcherTest : public AppControlsTestBase {
 public:
  AppActivityWatcherTest() = default;
  ~AppActivityWatcherTest() override = default;

  // AppControlsTestBase:
  void SetUp() override {
    AppControlsTestBase::SetUp();

    registry_ = std::make_unique<BlockedAppRegistry>(app_service_test().proxy(),
                                                     profile().GetPrefs());
  }

  void UpdateAppInstance(const std::string& app_id,
                         aura::Window* window,
                         apps::InstanceState state) {
    apps::InstanceParams params(app_id, window);
    params.state = std::make_pair(state, base::Time());
    instance_registry().CreateOrUpdateInstance(std::move(params));
  }

  BlockedAppRegistry* registry() { return registry_.get(); }

  apps::InstanceRegistry& instance_registry() {
    return app_service_test().proxy()->InstanceRegistry();
  }

 private:
  std::unique_ptr<BlockedAppRegistry> registry_;
};

TEST_F(AppActivityWatcherTest, BlockOnActive) {
  const std::string package_name = "com.example.app1", app_name = "app1";
  const std::string app_id = InstallArcApp(package_name, app_name);

  // Block the app.
  registry()->AddApp(app_id);

  // No local block dialog.
  EXPECT_FALSE(AppLocalBlockDialogView::GetActiveViewForTesting());

  // Simulate app running and being made active.
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  UpdateAppInstance(
      app_id, &window,
      static_cast<apps::InstanceState>(
          apps::InstanceState::kStarted | apps::InstanceState::kRunning |
          apps::InstanceState::kActive | apps::InstanceState::kVisible));

  // Local block dialog is created.
  EXPECT_TRUE(AppLocalBlockDialogView::GetActiveViewForTesting());

  // Close the dialog.
  AppLocalBlockDialogView::GetActiveViewForTesting()->AcceptDialog();
  task_environment()->RunUntilIdle();
}

}  // namespace ash::on_device_controls
