// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_util.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_terminal.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_icon.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/crostini_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_item_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace {

constexpr char kCrostiniAppLaunchHistogram[] = "Crostini.AppLaunch";
constexpr char kCrostiniAppNamePrefix[] = "_crostini_";
constexpr int64_t kDelayBeforeSpinnerMs = 400;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CrostiniAppLaunchAppType {
  // An app which isn't in the CrostiniAppRegistry. This shouldn't happen.
  kUnknownApp = 0,

  // The main terminal app.
  kTerminal = 1,

  // An app for which there is something in the CrostiniAppRegistry.
  kRegisteredApp = 2,

  kCount
};

void RecordAppLaunchHistogram(CrostiniAppLaunchAppType app_type) {
  base::UmaHistogramEnumeration(kCrostiniAppLaunchHistogram, app_type,
                                CrostiniAppLaunchAppType::kCount);
}

void OnLaunchFailed(const std::string& app_id) {
  // Remove the spinner so it doesn't stay around forever.
  // TODO(timloh): Consider also displaying a notification of some sort.
  ChromeLauncherController* chrome_controller =
      ChromeLauncherController::instance();
  DCHECK(chrome_controller);
  chrome_controller->GetShelfSpinnerController()->CloseSpinner(app_id);
}

void OnCrostiniRestarted(Profile* profile,
                         const std::string& app_id,
                         Browser* browser,
                         base::OnceClosure callback,
                         crostini::CrostiniResult result) {
  if (result != crostini::CrostiniResult::SUCCESS) {
    OnLaunchFailed(app_id);
    if (browser && browser->window())
      browser->window()->Close();
    if (result == crostini::CrostiniResult::OFFLINE_WHEN_UPGRADE_REQUIRED ||
        result == crostini::CrostiniResult::LOAD_COMPONENT_FAILED) {
      ShowCrostiniUpdateComponentView(profile,
                                      crostini::CrostiniUISurface::kAppList);
    }
    return;
  }
  std::move(callback).Run();
}

void OnApplicationLaunched(crostini::LaunchCrostiniAppCallback callback,
                           const std::string& app_id,
                           bool success) {
  if (!success)
    OnLaunchFailed(app_id);
  std::move(callback).Run(success, success ? "" : "Failed to launch " + app_id);
}

void OnSharePathForLaunchApplication(
    Profile* profile,
    const std::string& app_id,
    crostini::CrostiniRegistryService::Registration registration,
    const std::vector<std::string>& files,
    bool display_scaled,
    crostini::LaunchCrostiniAppCallback callback,
    bool success,
    const std::string& failure_reason) {
  if (!success) {
    OnLaunchFailed(app_id);
    return std::move(callback).Run(
        success, success ? ""
                         : "Failed to share paths to launch " + app_id + ":" +
                               failure_reason);
  }
  crostini::CrostiniManager::GetForProfile(profile)->LaunchContainerApplication(
      registration.VmName(), registration.ContainerName(),
      registration.DesktopFileId(), files, display_scaled,
      base::BindOnce(OnApplicationLaunched, std::move(callback), app_id));
}

void LaunchTerminal(Profile* profile,
                    apps::AppLaunchParams launch_params,
                    GURL vsh_in_crosh_url,
                    Browser* browser,
                    crostini::LaunchCrostiniAppCallback callback) {
  crostini::ShowContainerTerminal(profile, launch_params, vsh_in_crosh_url,
                                  browser);
  std::move(callback).Run(true, "");
}

void LaunchApplication(
    Profile* profile,
    const std::string& app_id,
    crostini::CrostiniRegistryService::Registration registration,
    int64_t display_id,
    const std::vector<storage::FileSystemURL>& files,
    bool display_scaled,
    crostini::LaunchCrostiniAppCallback callback) {
  ChromeLauncherController* chrome_launcher_controller =
      ChromeLauncherController::instance();
  DCHECK(chrome_launcher_controller);
  CrostiniAppWindowShelfController* shelf_controller =
      chrome_launcher_controller->crostini_app_window_shelf_controller();
  DCHECK(shelf_controller);
  shelf_controller->OnAppLaunchRequested(app_id, display_id);

  // Share any paths not in crostini.  The user will see the spinner while this
  // is happening.
  std::vector<base::FilePath> paths_to_share;
  std::vector<std::string> files_to_launch;
  for (const storage::FileSystemURL& url : files) {
    base::FilePath path;
    if (!file_manager::util::ConvertFileSystemURLToPathInsideCrostini(
            profile, url, &path)) {
      return std::move(callback).Run(false,
                                     "Invalid file: " + url.DebugString());
    }
    if (url.mount_filesystem_id() !=
        file_manager::util::GetCrostiniMountPointName(profile)) {
      paths_to_share.push_back(url.path());
    }
    files_to_launch.push_back(path.value());
  }

  if (paths_to_share.empty()) {
    OnSharePathForLaunchApplication(profile, app_id, std::move(registration),
                                    std::move(files_to_launch), display_scaled,
                                    std::move(callback), true, "");
  } else {
    guest_os::GuestOsSharePath::GetForProfile(profile)->SharePaths(
        registration.VmName(), std::move(paths_to_share), /*persist=*/false,
        base::BindOnce(OnSharePathForLaunchApplication, profile, app_id,
                       std::move(registration), std::move(files_to_launch),
                       display_scaled, std::move(callback)));
  }
}

// Helper class for loading icons. The callback is called when all icons have
// been loaded, or after a provided timeout, after which the object deletes
// itself.
// TODO(timloh): We should consider having a service, so multiple requests for
// the same icon won't load the same image multiple times and only the first
// request would incur the loading delay.
class IconLoadWaiter : public CrostiniAppIcon::Observer {
 public:
  static void LoadIcons(
      Profile* profile,
      const std::vector<std::string>& app_ids,
      int resource_size_in_dip,
      ui::ScaleFactor scale_factor,
      base::TimeDelta timeout,
      base::OnceCallback<void(const std::vector<gfx::ImageSkia>&)> callback) {
    new IconLoadWaiter(profile, app_ids, resource_size_in_dip, scale_factor,
                       timeout, std::move(callback));
  }

 private:
  IconLoadWaiter(
      Profile* profile,
      const std::vector<std::string>& app_ids,
      int resource_size_in_dip,
      ui::ScaleFactor scale_factor,
      base::TimeDelta timeout,
      base::OnceCallback<void(const std::vector<gfx::ImageSkia>&)> callback)
      : callback_(std::move(callback)) {
    for (const std::string& app_id : app_ids) {
      icons_.push_back(std::make_unique<CrostiniAppIcon>(
          profile, app_id, resource_size_in_dip, this));
      icons_.back()->LoadForScaleFactor(scale_factor);
    }

    timeout_timer_.Start(FROM_HERE, timeout, this,
                         &IconLoadWaiter::RunCallback);
  }

  // TODO(timloh): This is only called when an icon is found, so if any of the
  // requested apps are missing an icon, we'll have to wait for the timeout. We
  // should add an interface so we can avoid this.
  void OnIconUpdated(CrostiniAppIcon* icon) override {
    loaded_icons_++;
    if (loaded_icons_ != icons_.size())
      return;

    RunCallback();
  }

  void Delete() {
    DCHECK(!timeout_timer_.IsRunning());
    delete this;
  }

  void RunCallback() {
    DCHECK(callback_);
    std::vector<gfx::ImageSkia> result;
    for (const auto& icon : icons_)
      result.emplace_back(icon->image_skia());
    std::move(callback_).Run(result);

    // If we're running the callback as loading has finished, we can't delete
    // ourselves yet as it would destroy the CrostiniAppIcon which is calling
    // into us right now. If we hit the timeout, we delete immediately to avoid
    // any race with more icons finishing loading.
    if (timeout_timer_.IsRunning()) {
      timeout_timer_.AbandonAndStop();
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&IconLoadWaiter::Delete, base::Unretained(this)));
    } else {
      Delete();
    }
  }

  std::vector<std::unique_ptr<CrostiniAppIcon>> icons_;
  size_t loaded_icons_ = 0;

  base::OneShotTimer timeout_timer_;

  base::OnceCallback<void(const std::vector<gfx::ImageSkia>&)> callback_;
};

}  // namespace

namespace crostini {

ContainerId::ContainerId(std::string vm_name,
                         std::string container_name) noexcept
    : vm_name(std::move(vm_name)), container_name(std::move(container_name)) {}

bool operator<(const ContainerId& lhs, const ContainerId& rhs) noexcept {
  const auto result = lhs.vm_name.compare(rhs.vm_name);
  return result < 0 || (result == 0 && lhs.container_name < rhs.container_name);
}

std::ostream& operator<<(std::ostream& ostream,
                         const ContainerId& container_id) {
  return ostream << "(vm: \"" << container_id.vm_name << "\" container: \""
                 << container_id.container_name << "\")";
}

bool IsUninstallable(Profile* profile, const std::string& app_id) {
  if (!CrostiniFeatures::Get()->IsEnabled(profile) ||
      app_id == kCrostiniTerminalId) {
    return false;
  }
  CrostiniRegistryService* registry_service =
      CrostiniRegistryServiceFactory::GetForProfile(profile);
  base::Optional<CrostiniRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);
  if (registration)
    return registration->CanUninstall();
  return false;
}

bool IsCrostiniRunning(Profile* profile) {
  return crostini::CrostiniManager::GetForProfile(profile)->IsVmRunning(
      kCrostiniDefaultVmName);
}

bool ShouldConfigureDefaultContainer(Profile* profile) {
  const base::FilePath ansible_playbook_file_path =
      profile->GetPrefs()->GetFilePath(prefs::kCrostiniAnsiblePlaybookFilePath);
  return base::FeatureList::IsEnabled(
             features::kCrostiniAnsibleInfrastructure) &&
         !ansible_playbook_file_path.empty();
}

void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id) {
  LaunchCrostiniApp(profile, app_id, display_id, {}, base::DoNothing());
}

void AddSpinner(crostini::CrostiniManager::RestartId restart_id,
                const std::string& app_id,
                Profile* profile,
                std::string vm_name,
                std::string container_name) {
  ChromeLauncherController* chrome_controller =
      ChromeLauncherController::instance();
  if (chrome_controller &&
      crostini::CrostiniManager::GetForProfile(profile)->IsRestartPending(
          restart_id)) {
    chrome_controller->GetShelfSpinnerController()->AddSpinnerToShelf(
        app_id, std::make_unique<ShelfSpinnerItemController>(app_id));
  }
}

void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id,
                       const std::vector<storage::FileSystemURL>& files,
                       LaunchCrostiniAppCallback callback) {
  // Policies can change under us, and crostini may now be forbidden.
  if (!CrostiniFeatures::Get()->IsUIAllowed(profile)) {
    return std::move(callback).Run(false, "Crostini UI not allowed");
  }
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile);
  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile);
  base::Optional<crostini::CrostiniRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);
  if (!registration) {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kUnknownApp);
    return std::move(callback).Run(
        false, "LaunchCrostiniApp called with an unknown app_id: " + app_id);
  }

  // Store these as we move |registration| into LaunchContainerApplication().
  const std::string vm_name = registration->VmName();
  const std::string container_name = registration->ContainerName();

  base::OnceClosure launch_closure;
  Browser* browser = nullptr;
  if (app_id == kCrostiniTerminalId) {
    DCHECK(files.empty());
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kTerminal);

    // At this point, we know that Crostini UI is allowed.
    if (!crostini_manager->IsCrosTerminaInstalled() ||
        !profile->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled)) {
      ShowCrostiniInstallerView(profile, CrostiniUISurface::kAppList);
      return std::move(callback).Run(false, "Crostini not installed");
    }

    GURL vsh_in_crosh_url = GenerateVshInCroshUrl(
        profile, vm_name, container_name, std::vector<std::string>());
    apps::AppLaunchParams launch_params = GenerateTerminalAppLaunchParams();
    // Create the terminal here so it's created in the right display. If the
    // browser creation is delayed into the callback the root window for new
    // windows setting can be changed due to the launcher or shelf dismissal.
    Browser* browser =
        CreateContainerTerminal(profile, launch_params, vsh_in_crosh_url);
    launch_closure =
        base::BindOnce(&LaunchTerminal, profile, launch_params,
                       vsh_in_crosh_url, browser, std::move(callback));
  } else {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kRegisteredApp);
    launch_closure =
        base::BindOnce(&LaunchApplication, profile, app_id,
                       std::move(*registration), display_id, std::move(files),
                       registration->IsScaled(), std::move(callback));
  }

  // Update the last launched time and Termina version.
  registry_service->AppLaunched(app_id);
  crostini_manager->UpdateLaunchMetricsForEnterpriseReporting();

  auto restart_id = crostini_manager->RestartCrostini(
      vm_name, container_name,
      base::BindOnce(OnCrostiniRestarted, profile, app_id, browser,
                     std::move(launch_closure)));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AddSpinner, restart_id, app_id, profile, vm_name,
                     container_name),
      base::TimeDelta::FromMilliseconds(kDelayBeforeSpinnerMs));
}

void LoadIcons(Profile* profile,
               const std::vector<std::string>& app_ids,
               int resource_size_in_dip,
               ui::ScaleFactor scale_factor,
               base::TimeDelta timeout,
               base::OnceCallback<void(const std::vector<gfx::ImageSkia>&)>
                   icons_loaded_callback) {
  IconLoadWaiter::LoadIcons(profile, app_ids, resource_size_in_dip,
                            scale_factor, timeout,
                            std::move(icons_loaded_callback));
}

std::string CryptohomeIdForProfile(Profile* profile) {
  std::string id = chromeos::ProfileHelper::GetUserIdHashFromProfile(profile);
  // Empty id means we're running in a test.
  return id.empty() ? "test" : id;
}

std::string DefaultContainerUserNameForProfile(Profile* profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user) {
    return kCrostiniDefaultUsername;
  }
  return user->GetAccountName(/*use_display_email=*/false);
}

base::FilePath ContainerChromeOSBaseDirectory() {
  return base::FilePath("/mnt/chromeos");
}

std::string AppNameFromCrostiniAppId(const std::string& id) {
  return kCrostiniAppNamePrefix + id;
}

base::Optional<std::string> CrostiniAppIdFromAppName(
    const std::string& app_name) {
  if (!base::StartsWith(app_name, kCrostiniAppNamePrefix,
                        base::CompareCase::SENSITIVE)) {
    return base::nullopt;
  }
  return app_name.substr(strlen(kCrostiniAppNamePrefix));
}

void AddNewLxdContainerToPrefs(Profile* profile,
                               std::string vm_name,
                               std::string container_name) {
  auto* pref_service = profile->GetPrefs();

  base::Value new_container(base::Value::Type::DICTIONARY);
  new_container.SetKey(prefs::kVmKey, base::Value(vm_name));
  new_container.SetKey(prefs::kContainerKey, base::Value(container_name));

  ListPrefUpdate updater(pref_service, crostini::prefs::kCrostiniContainers);
  updater->Append(std::move(new_container));
}

void RemoveLxdContainerFromPrefs(Profile* profile,
                                 std::string vm_name,
                                 std::string container_name) {
  auto* pref_service = profile->GetPrefs();
  ListPrefUpdate updater(pref_service, crostini::prefs::kCrostiniContainers);
  updater->EraseListIter(std::find_if(
      updater->GetList().begin(), updater->GetList().end(),
      [&](const auto& dict) {
        return *dict.FindStringKey(prefs::kVmKey) == vm_name &&
               *dict.FindStringKey(prefs::kContainerKey) == container_name;
      }));

  CrostiniRegistryServiceFactory::GetForProfile(profile)->ClearApplicationList(
      vm_name, container_name);
  CrostiniMimeTypesServiceFactory::GetForProfile(profile)->ClearMimeTypes(
      vm_name, container_name);
}

base::string16 GetTimeRemainingMessage(base::TimeTicks start, int percent) {
  // Only estimate once we've spent at least 3 seconds OR gotten 10% of the way
  // through.
  constexpr base::TimeDelta kMinTimeForEstimate =
      base::TimeDelta::FromSeconds(3);
  constexpr base::TimeDelta kTimeDeltaZero = base::TimeDelta::FromSeconds(0);
  constexpr int kMinPercentForEstimate = 10;
  base::TimeDelta elapsed = base::TimeTicks::Now() - start;
  if ((elapsed >= kMinTimeForEstimate && percent > 0) ||
      (percent >= kMinPercentForEstimate && elapsed > kTimeDeltaZero)) {
    base::TimeDelta total_time_expected = (elapsed * 100) / percent;
    base::TimeDelta time_remaining = total_time_expected - elapsed;
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                                  ui::TimeFormat::LENGTH_SHORT, time_remaining);
  } else {
    return l10n_util::GetStringUTF16(
        IDS_CROSTINI_NOTIFICATION_OPERATION_STARTING);
  }
}

}  // namespace crostini
