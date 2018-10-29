// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_util.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/crostini/crostini_app_launch_observer.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_icon.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_item_controller.h"
#include "chrome/browser/ui/ash/window_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

constexpr char kCrostiniAppLaunchHistogram[] = "Crostini.AppLaunch";
constexpr char kCrostiniAppNamePrefix[] = "_crostini_";
constexpr int64_t kDelayBeforeSpinnerMs = 400;

// If true then override IsCrostiniUIAllowedForProfile and related methods to
// turn on Crostini.
bool g_crostini_ui_allowed_for_testing = false;

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
  chrome_controller->GetShelfSpinnerController()->Close(app_id);
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
    if (result == crostini::CrostiniResult::OFFLINE_WHEN_UPGRADE_REQUIRED) {
      ShowCrostiniUpgradeView(profile, crostini::CrostiniUISurface::kAppList);
    }
    return;
  }
  std::move(callback).Run();
}

void OnContainerApplicationLaunched(const std::string& app_id,
                                    crostini::CrostiniResult result) {
  if (result != crostini::CrostiniResult::SUCCESS)
    OnLaunchFailed(app_id);
}

Browser* CreateTerminal(const AppLaunchParams& launch_params,
                        const GURL& vsh_in_crosh_url) {
  return crostini::CrostiniManager::CreateContainerTerminal(launch_params,
                                                            vsh_in_crosh_url);
}

void ShowTerminal(const AppLaunchParams& launch_params,
                  const GURL& vsh_in_crosh_url,
                  Browser* browser) {
  crostini::CrostiniManager::ShowContainerTerminal(launch_params,
                                                   vsh_in_crosh_url, browser);
  browser->window()->GetNativeWindow()->SetProperty(
      kOverrideWindowIconResourceIdKey, IDR_LOGO_CROSTINI_TERMINAL);
}

void LaunchContainerApplication(
    Profile* profile,
    const std::string& app_id,
    crostini::CrostiniRegistryService::Registration registration,
    int64_t display_id,
    const std::vector<std::string>& files,
    bool display_scaled) {
  ChromeLauncherController* chrome_launcher_controller =
      ChromeLauncherController::instance();
  DCHECK_NE(chrome_launcher_controller, nullptr);
  CrostiniAppLaunchObserver* observer =
      chrome_launcher_controller->crostini_app_window_shelf_controller();
  DCHECK_NE(observer, nullptr);
  observer->OnAppLaunchRequested(app_id, display_id);
  crostini::CrostiniManager::GetForProfile(profile)->LaunchContainerApplication(
      registration.VmName(), registration.ContainerName(),
      registration.DesktopFileId(), files, display_scaled,
      base::BindOnce(OnContainerApplicationLaunched, app_id));
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

    timeout_timer_.AbandonAndStop();
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
    // into us right now.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&IconLoadWaiter::Delete, base::Unretained(this)));
  }

  std::vector<std::unique_ptr<CrostiniAppIcon>> icons_;
  size_t loaded_icons_ = 0;

  base::OneShotTimer timeout_timer_;

  base::OnceCallback<void(const std::vector<gfx::ImageSkia>&)> callback_;
};

}  // namespace

namespace crostini {

void SetCrostiniUIAllowedForTesting(bool enabled) {
  g_crostini_ui_allowed_for_testing = enabled;
}

bool IsCrostiniAllowedForProfile(Profile* profile) {
  if (g_crostini_ui_allowed_for_testing) {
    return true;
  }
  if (!profile || profile->IsChild() || profile->IsLegacySupervised() ||
      profile->IsOffTheRecord() ||
      chromeos::ProfileHelper::IsEphemeralUserProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile)) {
    return false;
  }
  if (!profile->GetPrefs()->GetBoolean(
          crostini::prefs::kUserCrostiniAllowedByPolicy)) {
    return false;
  }
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user->IsAffiliated() && !IsUnaffiliatedCrostiniAllowedByPolicy()) {
    return false;
  }
  if (!crostini::CrostiniManager::IsDevKvmPresent()) {
    // Hardware is physically incapable, no matter what the user wants.
    return false;
  }
  return virtual_machines::AreVirtualMachinesAllowedByVersionAndChannel() &&
         virtual_machines::AreVirtualMachinesAllowedByPolicy() &&
         base::FeatureList::IsEnabled(features::kCrostini);
}

bool IsCrostiniUIAllowedForProfile(Profile* profile) {
  if (g_crostini_ui_allowed_for_testing) {
    return true;
  }
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    return false;
  }

  return IsCrostiniAllowedForProfile(profile) &&
         base::FeatureList::IsEnabled(features::kExperimentalCrostiniUI);
}

bool IsCrostiniEnabled(Profile* profile) {
  return IsCrostiniUIAllowedForProfile(profile) &&
         profile->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled);
}

bool IsCrostiniRunning(Profile* profile) {
  return crostini::CrostiniManager::GetForProfile(profile)->IsVmRunning(
      kCrostiniDefaultVmName);
}

void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id) {
  LaunchCrostiniApp(profile, app_id, display_id, std::vector<std::string>());
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
                       const std::vector<std::string>& files) {
  // Policies can change under us, and crostini may now be forbidden.
  if (!IsCrostiniUIAllowedForProfile(profile)) {
    return;
  }
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile);
  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile);
  base::Optional<crostini::CrostiniRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);
  if (!registration) {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kUnknownApp);
    LOG(ERROR) << "LaunchCrostiniApp called with an unknown app_id: " << app_id;
    return;
  }

  // Store these as we move |registration| into LaunchContainerApplication().
  const std::string vm_name = registration->VmName();
  const std::string container_name = registration->ContainerName();

  base::OnceClosure launch_closure;
  Browser* browser = nullptr;
  if (app_id == kCrostiniTerminalId) {
    DCHECK(files.empty());
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kTerminal);

    if (!crostini_manager->IsCrosTerminaInstalled() ||
        !IsCrostiniEnabled(profile)) {
      ShowCrostiniInstallerView(profile, CrostiniUISurface::kAppList);
      return;
    }

    GURL vsh_in_crosh_url = crostini::CrostiniManager::GenerateVshInCroshUrl(
        profile, vm_name, container_name, std::vector<std::string>());
    AppLaunchParams launch_params =
        crostini::CrostiniManager::GenerateTerminalAppLaunchParams(profile);
    // Create the terminal here so it's created in the right display. If the
    // browser creation is delayed into the callback the root window for new
    // windows setting can be changed due to the launcher or shelf dismissal.
    Browser* browser = CreateTerminal(launch_params, vsh_in_crosh_url);
    launch_closure =
        base::BindOnce(&ShowTerminal, launch_params, vsh_in_crosh_url, browser);
  } else {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kRegisteredApp);
    launch_closure = base::BindOnce(
        &LaunchContainerApplication, profile, app_id, std::move(*registration),
        display_id, std::move(files), registration->IsScaled());
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

std::string ContainerUserNameForProfile(Profile* profile) {
  // Get rid of the @domain.name in the profile user name (an email address).
  std::string container_username = profile->GetProfileUserName();
  if (container_username.find('@') != std::string::npos) {
    // gaia::CanonicalizeEmail CHECKs its argument contains'@'.
    container_username = gaia::CanonicalizeEmail(container_username);
    // |container_username| may have changed, so we have to find again.
    return container_username.substr(0, container_username.find('@'));
  }
  return container_username;
}

base::FilePath ContainerHomeDirectoryForProfile(Profile* profile) {
  return base::FilePath("/home/" + ContainerUserNameForProfile(profile));
}

base::FilePath ContainerChromeOSBaseDirectory() {
  return base::FilePath("/ChromeOS/");
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

bool IsUnaffiliatedCrostiniAllowedByPolicy() {
  bool unaffiliated_crostini_allowed;
  if (chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kDeviceUnaffiliatedCrostiniAllowed,
          &unaffiliated_crostini_allowed)) {
    return unaffiliated_crostini_allowed;
  }
  // If device policy is not set, allow Crostini.
  return true;
}

}  // namespace crostini
