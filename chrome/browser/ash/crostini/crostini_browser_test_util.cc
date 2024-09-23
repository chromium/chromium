// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_browser_test_util.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_wayland_server.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

// ChromeBrowserMainExtraParts used to install a FakeComponentManagerAsh.
class CrostiniBrowserTestChromeBrowserMainExtraParts
    : public ChromeBrowserMainExtraParts {
 public:
  explicit CrostiniBrowserTestChromeBrowserMainExtraParts(bool register_termina)
      : register_termina_(register_termina) {}

  CrostiniBrowserTestChromeBrowserMainExtraParts(
      const CrostiniBrowserTestChromeBrowserMainExtraParts&) = delete;
  CrostiniBrowserTestChromeBrowserMainExtraParts& operator=(
      const CrostiniBrowserTestChromeBrowserMainExtraParts&) = delete;

  component_updater::FakeComponentManagerAsh* component_manager_ash() {
    return component_manager_ash_ptr_;
  }

  content::NetworkConnectionChangeSimulator* connection_change_simulator() {
    return &connection_change_simulator_;
  }

  // ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override {
    auto component_manager_ash =
        base::MakeRefCounted<component_updater::FakeComponentManagerAsh>();
    component_manager_ash->set_supported_components(
        {imageloader::kTerminaComponentName});

    if (register_termina_) {
      component_manager_ash->SetRegisteredComponents(
          {imageloader::kTerminaComponentName});
      component_manager_ash->ResetComponentState(
          imageloader::kTerminaComponentName,
          component_updater::FakeComponentManagerAsh::ComponentInfo(
              component_updater::ComponentManagerAsh::Error::NONE,
              base::FilePath("/dev/null"), base::FilePath("/dev/null")));
    }
    component_manager_ash_ptr_ = component_manager_ash.get();

    browser_process_platform_part_test_api_ =
        std::make_unique<BrowserProcessPlatformPartTestApi>(
            g_browser_process->platform_part());
    browser_process_platform_part_test_api_->InitializeComponentManager(
        std::move(component_manager_ash));
  }
  // Ideally we'd call SetConnectionType in PostCreateThreads, but currently we
  // have to wait for PreProfileInit to complete, since that creatse the
  // ash::Shell that AshService needs in order to start.
  void PostProfileInit(Profile* profile, bool is_initial_profile) override {
    // The setup below is intended to run for only the initial profile.
    if (!is_initial_profile) {
      return;
    }

    connection_change_simulator_.SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_WIFI);
  }
  void PostMainMessageLoopRun() override {
    component_manager_ash_ptr_ = nullptr;
    browser_process_platform_part_test_api_->ShutdownComponentManager();
    browser_process_platform_part_test_api_.reset();
  }

 private:
  const bool register_termina_;

  std::unique_ptr<BrowserProcessPlatformPartTestApi>
      browser_process_platform_part_test_api_;
  raw_ptr<component_updater::FakeComponentManagerAsh>
      component_manager_ash_ptr_ = nullptr;

  content::NetworkConnectionChangeSimulator connection_change_simulator_;
};

CrostiniBrowserTestBase::CrostiniBrowserTestBase(bool register_termina)
    : register_termina_(register_termina) {
  scoped_feature_list_.InitAndEnableFeature(features::kCrostini);
  fake_crostini_features_.SetAll(true);

  dmgr_ = new ash::disks::MockDiskMountManager;
  ON_CALL(*dmgr_, MountPath)
      .WillByDefault(Invoke(this, &CrostiniBrowserTestBase::DiskMountImpl));
  // Test object will be deleted by DiskMountManager::Shutdown
  ash::disks::DiskMountManager::InitializeForTesting(dmgr_);
}
void CrostiniBrowserTestBase::DiskMountImpl(
    const std::string& source_path,
    const std::string& source_format,
    const std::string& mount_label,
    const std::vector<std::string>& mount_options,
    ash::MountType type,
    ash::MountAccessMode access_mode,
    ash::disks::DiskMountManager::MountPathCallback callback) {
  const ash::disks::DiskMountManager::MountPoint info{source_path,
                                                      "/path/to/mount", type};
  std::move(callback).Run(ash::MountError::kSuccess, info);
  dmgr_->NotifyMountEvent(ash::disks::DiskMountManager::MountEvent::MOUNTING,
                          ash::MountError::kSuccess, info);
}

void CrostiniBrowserTestBase::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  ChromeBrowserMainParts* chrome_browser_main_parts =
      static_cast<ChromeBrowserMainParts*>(browser_main_parts);
  extra_parts_ =
      new CrostiniBrowserTestChromeBrowserMainExtraParts(register_termina_);
  chrome_browser_main_parts->AddParts(base::WrapUnique(extra_parts_.get()));
}

void CrostiniBrowserTestBase::SetUpOnMainThread() {
  browser()->profile()->GetPrefs()->SetBoolean(
      crostini::prefs::kCrostiniEnabled, true);
}

void CrostiniBrowserTestBase::SetConnectionType(
    network::mojom::ConnectionType connection_type) {
  extra_parts_->connection_change_simulator()->SetConnectionType(
      connection_type);
}

void CrostiniBrowserTestBase::UnregisterTermina() {
  extra_parts_->component_manager_ash()->ResetComponentState(
      imageloader::kTerminaComponentName,
      component_updater::FakeComponentManagerAsh::ComponentInfo(
          component_updater::ComponentManagerAsh::Error::INSTALL_FAILURE,
          base::FilePath(), base::FilePath()));
}
