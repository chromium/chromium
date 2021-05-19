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
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/system_tray_client_impl.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "components/component_updater/component_updater_service.h"

namespace crosapi {

namespace {

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
constexpr ComponentInfo kLacrosDogfoodCanaryInfo = {
    "lacros-dogfood-canary", "hkifppleldbgkdlijbdfkdpedggaopda"};
constexpr ComponentInfo kLacrosDogfoodDevInfo = {
    "lacros-dogfood-dev", "ldobopbhiamakmncndpkeelenhdmgfhk"};
constexpr ComponentInfo kLacrosDogfoodStableInfo = {
    "lacros-dogfood-stable", "hnfmbeciphpghlfgpjfbcdifbknombnk"};

// The rootfs lacros-chrome binary related files.
constexpr char kLacrosChromeBinary[] = "chrome";
constexpr char kLacrosImage[] = "lacros.squash";
constexpr char kLacrosMetadata[] = "metadata.json";

// The rootfs lacros-chrome binary related paths.
// Must be kept in sync with lacros upstart conf files.
constexpr char kRootfsLacrosMountPoint[] = "/run/lacros";
constexpr char kRootfsLacrosPath[] = "/opt/google/lacros";

// Lacros upstart jobs for mounting/unmounting the lacros-chrome image.
// The conversion of upstart job names to dbus object paths is undocumented. See
// function nih_dbus_path in libnih for the implementation.
constexpr char kLacrosMounterUpstartJob[] = "lacros_2dmounter";
constexpr char kLacrosUnmounterUpstartJob[] = "lacros_2dunmounter";

ComponentInfo GetLacrosComponentInfo() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(browser_util::kLacrosStabilitySwitch)) {
    std::string value =
        cmdline->GetSwitchValueASCII(browser_util::kLacrosStabilitySwitch);
    if (value == browser_util::kLacrosStabilityLeastStable)
      return kLacrosDogfoodCanaryInfo;
    if (value == browser_util::kLacrosStabilityLessStable)
      return kLacrosDogfoodDevInfo;
    if (value == browser_util::kLacrosStabilityMoreStable)
      return kLacrosDogfoodStableInfo;
  }
  // Use once a week / Dev style updates by default.
  return kLacrosDogfoodDevInfo;
}

std::string GetLacrosComponentName() {
  return GetLacrosComponentInfo().name;
}

// Returns the CRX "extension" ID for a lacros component.
std::string GetLacrosComponentCrxId() {
  return GetLacrosComponentInfo().crx_id;
}

// Returns whether lacros-chrome component is already installed.
bool CheckInstalledMayBlock(
    scoped_refptr<component_updater::CrOSComponentManager> manager) {
  return manager->IsRegisteredMayBlock(GetLacrosComponentName());
}

// Returns whether lacros-fishfood component is already installed.
// If it is, delete the user directory, too, because it will be
// uninstalled.
bool CheckInstalledAndMaybeRemoveUserDirectory(
    scoped_refptr<component_updater::CrOSComponentManager> manager) {
  if (!CheckInstalledMayBlock(manager))
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
      SystemTrayClientImpl::Get()->SetLacrosUpdateAvailable();
    }
  }
};

}  // namespace

BrowserLoader::BrowserLoader(
    scoped_refptr<component_updater::CrOSComponentManager> manager)
    : BrowserLoader(std::make_unique<DelegateImpl>(),
                    manager,
                    g_browser_process->component_updater(),
                    chromeos::UpstartClient::Get()) {}

BrowserLoader::BrowserLoader(
    std::unique_ptr<Delegate> delegate,
    scoped_refptr<component_updater::CrOSComponentManager> manager,
    component_updater::ComponentUpdateService* updater,
    chromeos::UpstartClient* upstart_client)
    : delegate_(std::move(delegate)),
      component_manager_(manager),
      component_update_service_(updater),
      upstart_client_(upstart_client) {
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

  // If the user has specified to force using stateful or rootfs lacros-chrome
  // binary, force the selection.
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(browser_util::kLacrosSelectionSwitch)) {
    auto value =
        cmdline->GetSwitchValueASCII(browser_util::kLacrosSelectionSwitch);
    if (value == browser_util::kLacrosSelectionRootfs) {
      LoadRootfsLacros(std::move(callback));
      return;
    }
    if (value == browser_util::kLacrosSelectionStateful) {
      LoadStatefulLacros(std::move(callback));
      return;
    }
  }

  // TODO(b/188473251): Remove this check once rootfs lacros-chrome is in.
  if (!base::PathExists(
          base::FilePath(kRootfsLacrosPath).Append(kLacrosImage))) {
    LOG(ERROR) << "Rootfs lacros image is missing. Going to load lacros "
                  "component instead.";
    LoadStatefulLacros(std::move(callback));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CheckInstalledMayBlock, component_manager_),
      base::BindOnce(&BrowserLoader::OnLoadSelection,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserLoader::OnLoadSelection(LoadCompletionCallback callback,
                                    bool was_installed) {
  // If there currently isn't a stateful lacros-chrome binary, proceed to use
  // the rootfs lacros-chrome binary and start the installation of the
  // stateful lacros-chrome binary in the background.
  if (!was_installed) {
    LoadRootfsLacros(std::move(callback));
    LoadStatefulLacros({});
    return;
  }

  // Otherwise proceed to compare the lacros-chrome binary versions.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&browser_util::GetRootfsLacrosVersionMayBlock,
                     base::FilePath(kRootfsLacrosPath).Append(kLacrosMetadata)),
      base::BindOnce(&BrowserLoader::OnLoadVersionSelection,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserLoader::OnLoadVersionSelection(
    LoadCompletionCallback callback,
    base::Version rootfs_lacros_version) {
  // Compare the rootfs vs stateful lacros-chrome binary versions.
  // If the rootfs lacros-chrome is greater than or equal to the stateful
  // lacros-chrome version, prioritize using the rootfs lacros-chrome and let
  // stateful lacros-chrome update in the background.
  if (rootfs_lacros_version.IsValid()) {
    auto lacros_component_name = GetLacrosComponentName();
    for (const auto& component_info :
         component_update_service_->GetComponents()) {
      if (component_info.id != lacros_component_name)
        continue;
      if (component_info.version <= rootfs_lacros_version) {
        LOG(WARNING) << "Stateful lacros version ("
                     << component_info.version.GetString()
                     << ") is older or same as the rootfs lacros version ("
                     << rootfs_lacros_version.GetString()
                     << ", proceeding to use rootfs lacros.";
        LoadRootfsLacros(std::move(callback));
        LoadStatefulLacros({});
        return;
      }
      // Break out to use stateful lacros-chrome.
      LOG(WARNING)
          << "Stateful lacros version is newer than the one in rootfs, "
          << "procceding to use stateful lacros.";
      break;
    }
  }
  LoadStatefulLacros(std::move(callback));
}

void BrowserLoader::LoadStatefulLacros(LoadCompletionCallback callback) {
  LOG(WARNING) << "Loading stateful lacros.";
  // Unmount the rootfs lacros-chrome if we want to use stateful lacros-chrome.
  // This will keep stateful lacros-chrome only mounted and not hold the rootfs
  // lacros-chrome mount until a `Unload`.
  if (callback && base::PathExists(base::FilePath(kRootfsLacrosMountPoint)
                                       .Append(kLacrosChromeBinary))) {
    // Ignore the unmount result.
    upstart_client_->StartJob(kLacrosUnmounterUpstartJob, {},
                              base::BindOnce([](bool) {}));
  }
  component_manager_->Load(
      GetLacrosComponentName(),
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      // If a compatible installation exists, use that and download any updates
      // in the background.
      component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
      // If `callback` is null, means stateful lacros-chrome should be
      // installed/updated but rootfs lacros-chrome will be used.
      callback
          ? base::BindOnce(&BrowserLoader::OnLoadComplete,
                           weak_factory_.GetWeakPtr(), std::move(callback))
          : base::BindOnce([](component_updater::CrOSComponentManager::Error,
                              const base::FilePath& path) {}));
}

void BrowserLoader::LoadRootfsLacros(LoadCompletionCallback callback) {
  LOG(WARNING) << "Loading rootfs lacros.";
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &base::PathExists,
          base::FilePath(kRootfsLacrosMountPoint).Append(kLacrosChromeBinary)),
      base::BindOnce(&BrowserLoader::OnLoadRootfsLacros,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserLoader::OnLoadRootfsLacros(LoadCompletionCallback callback,
                                       bool already_mounted) {
  if (already_mounted) {
    OnUpstartLacrosMounter(std::move(callback), true);
    return;
  }
  upstart_client_->StartJob(
      kLacrosMounterUpstartJob, {},
      base::BindOnce(&BrowserLoader::OnUpstartLacrosMounter,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserLoader::OnUpstartLacrosMounter(LoadCompletionCallback callback,
                                           bool success) {
  if (!success)
    LOG(WARNING) << "Upstart failed to mount rootfs lacros.";
  OnLoadComplete(
      std::move(callback), component_updater::CrOSComponentManager::Error::NONE,
      // If mounting wasn't successful, return a empty mount point to indicate
      // failure. `OnLoadComplete` handles empty mount points and forwards the
      // errors on the return callbacks.
      success ? base::FilePath(kRootfsLacrosMountPoint) : base::FilePath());
}

void BrowserLoader::Unload() {
  // Can be called even if Lacros isn't enabled, to clean up the old install.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CheckInstalledAndMaybeRemoveUserDirectory,
                     component_manager_),
      base::BindOnce(&BrowserLoader::OnCheckInstalled,
                     weak_factory_.GetWeakPtr()));
  // Unmount the rootfs lacros-chrome if it was mounted.
  // Ignore the unmount result.
  upstart_client_->StartJob(kLacrosUnmounterUpstartJob, {},
                            base::BindOnce([](bool) {}));
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
  // Bail out on error or empty `path`.
  if (error != component_updater::CrOSComponentManager::Error::NONE ||
      path.empty()) {
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
