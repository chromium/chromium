// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_loader.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "components/component_updater/component_updater_service.h"

namespace crosapi {

namespace {

// The Lacros dogfood is the logical successor to the Lacros fishfood. They are
// no intrinsic differences other than a slight change to the app ids used for
// deployment. This feature is a temporary measure to ensure that when the new
// app ids are ready, ash can be immediately switched to the dogfood deployment.
// At that point, this feature can only be removed from the code and we can
// switch unconditionally to the dogfood deployment..
const base::Feature kLacrosPreferDogfoodOverFishfood{
    "LacrosPreferDogfoodOverFishfood", base::FEATURE_ENABLED_BY_DEFAULT};

// Emergency kill switch in case the notification code doesn't work properly.
const base::Feature kLacrosShowUpdateNotifications{
    "LacrosShowUpdateNotifications", base::FEATURE_ENABLED_BY_DEFAULT};

struct ComponentInfo {
  // The client-side component name.
  const char* const name;
  // The CRX "extension" ID for component updater.
  // Must match the Omaha console.
  const char* const crx_id;
};

// NOTE: If you change the lacros component names, you must also update
// chrome/browser/component_updater/cros_component_installer_chromeos.cc
constexpr ComponentInfo kLacrosFishfoodInfo = {
    "lacros-fishfood", "hkifppleldbgkdlijbdfkdpedggaopda"};
constexpr ComponentInfo kLacrosDogfoodDevInfo = {
    "lacros-dogfood-dev", "ldobopbhiamakmncndpkeelenhdmgfhk"};
constexpr ComponentInfo kLacrosDogfoodStableInfo = {
    "lacros-dogfood-stable", "hnfmbeciphpghlfgpjfbcdifbknombnk"};

ComponentInfo GetLacrosComponentInfo() {
  if (!base::FeatureList::IsEnabled(kLacrosPreferDogfoodOverFishfood))
    return kLacrosFishfoodInfo;

  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(browser_util::kLacrosStabilitySwitch)) {
    std::string value =
        cmdline->GetSwitchValueASCII(browser_util::kLacrosStabilitySwitch);
    if (value == browser_util::kLacrosStabilityLessStable) {
      return kLacrosDogfoodDevInfo;
    } else if (value == browser_util::kLacrosStabilityMoreStable) {
      return kLacrosDogfoodStableInfo;
    }
  }
  // Use more frequent updates by default.
  return kLacrosDogfoodDevInfo;
}

std::string GetLacrosComponentName() {
  return GetLacrosComponentInfo().name;
}

// Returns the CRX "extension" ID for a lacros component.
std::string GetLacrosComponentCrxId() {
  return GetLacrosComponentInfo().crx_id;
}

// Returns whether lacros-fishfood component is already installed.
// If it is, delete the user directory, too, because it will be
// uninstalled.
bool CheckInstalledAndMaybeRemoveUserDirectory(
    scoped_refptr<component_updater::CrOSComponentManager> manager) {
  if (!manager->IsRegisteredMayBlock(GetLacrosComponentName()))
    return false;

  // Since we're already on a background thread, delete the user-data-dir
  // associated with lacros.
  // TODO(hidehiko): This approach has timing issue. Specifically, if Chrome
  // shuts down during the directory remove, some partially-removed directory
  // may be kept, and if the user flips the flag in the next time, that
  // partially-removed directory could be used. Fix this.
  base::DeletePathRecursively(browser_util::GetUserDataDir());
  return true;
}

// Production delegate implementation.
class DelegateImpl : public BrowserLoader::Delegate {
 public:
  DelegateImpl() = default;
  DelegateImpl(const DelegateImpl&) = delete;
  DelegateImpl& operator=(const DelegateImpl&) = delete;
  ~DelegateImpl() override = default;

  // BrowserLoader::Delegate:
  void SetLacrosUpdateAvailable() override {
    if (base::FeatureList::IsEnabled(kLacrosShowUpdateNotifications)) {
      // Show the update notification in ash.
      SystemTrayClient::Get()->SetLacrosUpdateAvailable();
    }
  }
};

}  // namespace

BrowserLoader::BrowserLoader(
    scoped_refptr<component_updater::CrOSComponentManager> manager)
    : BrowserLoader(std::make_unique<DelegateImpl>(), manager) {}

BrowserLoader::BrowserLoader(
    std::unique_ptr<Delegate> delegate,
    scoped_refptr<component_updater::CrOSComponentManager> manager)
    : delegate_(std::move(delegate)),
      component_manager_(manager),
      component_update_service_(g_browser_process->component_updater()) {
  DCHECK(delegate_);
  DCHECK(component_manager_);
}

BrowserLoader::~BrowserLoader() {
  // May be null in tests.
  if (component_update_service_) {
    // Removing an observer is a no-op if the observer wasn't added.
    component_update_service_->RemoveObserver(this);
  }
}

void BrowserLoader::Load(LoadCompletionCallback callback) {
  DCHECK(browser_util::IsLacrosEnabled());

  // TODO(crbug.com/1078607): Remove non-error logging from this class.
  LOG(WARNING) << "Starting lacros component load.";

  // If the user has specified a path for the lacros-chrome binary, use that
  // rather than component manager.
  base::FilePath lacros_chrome_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          chromeos::switches::kLacrosChromePath);
  if (!lacros_chrome_path.empty()) {
    OnLoadComplete(std::move(callback),
                   component_updater::CrOSComponentManager::Error::NONE,
                   lacros_chrome_path);
    return;
  }

  component_manager_->Load(
      GetLacrosComponentName(),
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      // If a compatible installation exists, use that and download any updates
      // in the background.
      component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(&BrowserLoader::OnLoadComplete, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void BrowserLoader::Unload() {
  // Can be called even if Lacros isn't enabled, to clean up the old install.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CheckInstalledAndMaybeRemoveUserDirectory,
                     component_manager_),
      base::BindOnce(&BrowserLoader::OnCheckInstalled,
                     weak_factory_.GetWeakPtr()));
}

void BrowserLoader::OnEvent(Events event, const std::string& id) {
  // Check for the Lacros component being updated.
  if (event == Events::COMPONENT_UPDATED && id == GetLacrosComponentCrxId()) {
    delegate_->SetLacrosUpdateAvailable();
  }
}

void BrowserLoader::OnLoadComplete(
    LoadCompletionCallback callback,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  // Bail out on error.
  if (error != component_updater::CrOSComponentManager::Error::NONE) {
    LOG(WARNING) << "Error loading lacros component image: "
                 << static_cast<int>(error);
    std::move(callback).Run(base::FilePath());
    return;
  }
  // Log the path on success.
  LOG(WARNING) << "Loaded lacros image at " << path.MaybeAsASCII();
  std::move(callback).Run(path);

  // May be null in tests.
  if (component_update_service_) {
    // Now that we have the initial component download, start observing for
    // future updates. We don't do this in the constructor because we don't want
    // to show the "update available" notification for the initial load.
    component_update_service_->AddObserver(this);
  }
}

void BrowserLoader::OnCheckInstalled(bool was_installed) {
  if (!was_installed)
    return;

  // Workaround for login crash when the user un-sets the LacrosSupport flag.
  // CrOSComponentManager::Unload() calls into code in MetadataTable that
  // assumes that system salt is available. This isn't always true when chrome
  // restarts to apply non-owner flags. It's hard to make MetadataTable async.
  // Ensure salt is available before unloading. https://crbug.com/1122674
  chromeos::SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &BrowserLoader::UnloadAfterCleanUp, weak_factory_.GetWeakPtr()));
}

void BrowserLoader::UnloadAfterCleanUp(const std::string& ignored_salt) {
  CHECK(chromeos::SystemSaltGetter::Get()->GetRawSalt());
  component_manager_->Unload(GetLacrosComponentName());
}

}  // namespace crosapi
