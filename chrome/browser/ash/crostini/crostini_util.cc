// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_util.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_crostini_tracker.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_item_controller.h"
#include "chrome/browser/ui/views/crostini/crostini_recovery_view.h"
#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace crostini {
namespace {

constexpr char kCrostiniAppLaunchHistogram[] = "Crostini.AppLaunch";
constexpr char kCrostiniAppLaunchResultHistogram[] = "Crostini.AppLaunchResult";
constexpr char kCrostiniAppLaunchResultHistogramTerminal[] =
    "Crostini.AppLaunchResult.Terminal";
constexpr char kCrostiniAppLaunchResultHistogramRegistered[] =
    "Crostini.AppLaunchResult.Registered";
constexpr char kCrostiniAppLaunchResultHistogramUnknown[] =
    "Crostini.AppLaunchResult.Unknown";
constexpr int64_t kDelayBeforeSpinnerMs = 400;

void OnApplicationLaunched(const std::string& app_id,
                           crostini::CrostiniSuccessCallback callback,
                           const crostini::CrostiniResult failure_result,
                           bool success,
                           const std::string& failure_reason) {
  CrostiniAppLaunchAppType type = CrostiniAppLaunchAppType::kRegisteredApp;
  CrostiniResult result = success ? CrostiniResult::SUCCESS : failure_result;
  RecordAppLaunchResultHistogram(type, result);
  std::move(callback).Run(success, failure_reason);
}

void OnLaunchFailed(const std::string& app_id,
                    crostini::CrostiniSuccessCallback callback,
                    const std::string& failure_reason,
                    crostini::CrostiniResult result) {
  // Remove the spinner and icon. Controller doesn't exist in tests.
  // TODO(timloh): Consider also displaying a notification for failure.
  if (auto* chrome_controller = ChromeShelfController::instance()) {
    chrome_controller->GetShelfSpinnerController()->CloseSpinner(app_id);
  }
  OnApplicationLaunched(app_id, std::move(callback), result, false,
                        failure_reason);
}

void OnSharePathForLaunchApplication(
    Profile* profile,
    const std::string& app_id,
    guest_os::GuestOsRegistryService::Registration registration,
    const guest_os::GuestId& container_id,
    int64_t display_id,
    const std::vector<std::string>& args,
    crostini::CrostiniSuccessCallback callback,
    bool success,
    const std::string& failure_reason) {
  if (!success) {
    return OnLaunchFailed(
        app_id, std::move(callback),
        "Failed to share paths to launch " + app_id + ": " + failure_reason,
        CrostiniResult::SHARE_PATHS_FAILED);
  }

  guest_os::launcher::LaunchApplication(
      profile, container_id, std::move(registration), display_id, args,
      base::BindOnce(OnApplicationLaunched, app_id, std::move(callback),
                     crostini::CrostiniResult::UNKNOWN_ERROR));
}

void LaunchApplication(
    Profile* profile,
    const std::string& app_id,
    guest_os::GuestOsRegistryService::Registration registration,
    const guest_os::GuestId& container_id,
    int64_t display_id,
    const std::vector<guest_os::LaunchArg>& args,
    crostini::CrostiniSuccessCallback callback) {
  ChromeShelfController* chrome_shelf_controller =
      ChromeShelfController::instance();
  DCHECK(chrome_shelf_controller);

  AppServiceAppWindowShelfController* app_service_controller =
      chrome_shelf_controller->app_service_app_window_controller();
  DCHECK(app_service_controller);

  AppServiceAppWindowCrostiniTracker* crostini_tracker =
      app_service_controller->app_service_crostini_tracker();
  DCHECK(crostini_tracker);

  crostini_tracker->OnAppLaunchRequested(app_id, display_id);

  // Get vm_info because we need seneschal_server_handle.
  const std::string& vm_name = registration.VmName();
  auto vm_info =
      guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile)->GetVmInfo(
          vm_name);
  if (!vm_info) {
    return OnLaunchFailed(app_id, std::move(callback),
                          "Crostini VM not running: " + vm_name,
                          CrostiniResult::SHARE_PATHS_FAILED);
  }

  // Share any paths not in crostini.  The user will see the spinner while this
  // is happening.
  auto* share_path = guest_os::GuestOsSharePathFactory::GetForProfile(profile);
  auto paths_or_error = share_path->ConvertArgsToPathsToShare(
      registration, args, crostini::ContainerChromeOSBaseDirectory(),
      /*map_crostini_home=*/true);
  if (absl::holds_alternative<std::string>(paths_or_error)) {
    OnLaunchFailed(app_id, std::move(callback),
                   absl::get<std::string>(paths_or_error),
                   CrostiniResult::SHARE_PATHS_FAILED);
    return;
  }
  const auto& paths =
      absl::get<guest_os::GuestOsSharePath::PathsToShare>(paths_or_error);
  share_path->SharePaths(
      vm_name, vm_info->seneschal_server_handle(),
      std::move(paths.paths_to_share),
      base::BindOnce(OnSharePathForLaunchApplication, profile, app_id,
                     std::move(registration), container_id, display_id,
                     std::move(paths.launch_args), std::move(callback)));
}

}  // namespace

bool IsUninstallable(Profile* profile, const std::string& app_id) {
  if (!CrostiniFeatures::Get()->IsEnabled(profile)) {
    return false;
  }
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  std::optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);
  if (registration) {
    return registration->CanUninstall();
  }
  return false;
}

bool IsCrostiniRunning(Profile* profile) {
  auto* manager = crostini::CrostiniManager::GetForProfile(profile);
  return manager && manager->IsVmRunning(kCrostiniDefaultVmName);
}

bool ShouldConfigureDefaultContainer(Profile* profile) {
  const base::FilePath ansible_playbook_file_path =
      profile->GetPrefs()->GetFilePath(prefs::kCrostiniAnsiblePlaybookFilePath);
  bool default_container_configured = profile->GetPrefs()->GetBoolean(
      prefs::kCrostiniDefaultContainerConfigured);
  return base::FeatureList::IsEnabled(
             features::kCrostiniAnsibleInfrastructure) &&
         !default_container_configured && !ansible_playbook_file_path.empty();
}

bool ShouldAllowContainerUpgrade(Profile* profile) {
  return CrostiniFeatures::Get()->IsContainerUpgradeUIAllowed(profile) &&
         crostini::CrostiniManager::GetForProfile(profile)
             ->IsContainerUpgradeable(DefaultContainerId());
}

void AddSpinner(crostini::CrostiniManager::RestartId restart_id,
                const std::string& app_id,
                Profile* profile) {
  ChromeShelfController* chrome_controller = ChromeShelfController::instance();
  if (chrome_controller &&
      crostini::CrostiniManager::GetForProfile(profile)->IsRestartPending(
          restart_id)) {
    chrome_controller->GetShelfSpinnerController()->AddSpinnerToShelf(
        app_id, std::make_unique<ShelfSpinnerItemController>(app_id));
  }
}

void LaunchCrostiniAppImpl(
    Profile* profile,
    const std::string& app_id,
    guest_os::GuestOsRegistryService::Registration registration,
    const guest_os::GuestId& container_id,
    int64_t display_id,
    const std::vector<guest_os::LaunchArg>& args,
    CrostiniSuccessCallback callback) {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile);
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kRegisteredApp);

  // Update the last launched time and Termina version.
  registry_service->AppLaunched(app_id);
  crostini_manager->UpdateLaunchMetricsForEnterpriseReporting();

  auto spinner_app_id =
      registration.Terminal() ? guest_os::kTerminalSystemAppId : app_id;
  auto restart_id = crostini_manager->RestartCrostini(
      container_id,
      base::BindOnce(
          [](Profile* profile, const std::string& app_id,
             guest_os::GuestOsRegistryService::Registration registration,
             const guest_os::GuestId& container_id, int64_t display_id,
             const std::vector<guest_os::LaunchArg> args,
             crostini::CrostiniSuccessCallback callback,
             crostini::CrostiniResult result) {
            if (result != crostini::CrostiniResult::SUCCESS) {
              OnLaunchFailed(app_id, std::move(callback),
                             base::StringPrintf(
                                 "crostini restart to launch app %s failed: %d",
                                 app_id.c_str(), static_cast<int>(result)),
                             result);
              return;
            }

            LaunchApplication(profile, app_id, std::move(registration),
                              container_id, display_id, args,
                              std::move(callback));
          },
          profile, app_id, std::move(registration), container_id, display_id,
          args, std::move(callback)));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AddSpinner, restart_id, spinner_app_id, profile),
      base::Milliseconds(kDelayBeforeSpinnerMs));
}

void LaunchCrostiniAppWithIntent(Profile* profile,
                                 const std::string& app_id,
                                 int64_t display_id,
                                 apps::IntentPtr intent,
                                 const std::vector<guest_os::LaunchArg>& args,
                                 CrostiniSuccessCallback callback) {
  // Policies can change under us, and crostini may now be forbidden.
  std::string reason;
  if (!CrostiniFeatures::Get()->IsAllowedNow(profile, &reason)) {
    LOG(ERROR) << "Crostini not allowed: " << reason;
    return std::move(callback).Run(false, "Crostini UI not allowed");
  }

  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile);
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  std::optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);

  if (!registration) {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kUnknownApp);
    RecordAppLaunchResultHistogram(CrostiniAppLaunchAppType::kUnknownApp,
                                   CrostiniResult::UNREGISTERED_APPLICATION);
    return std::move(callback).Run(
        false, "LaunchCrostiniApp called with an unknown app_id: " + app_id);
  }
  guest_os::GuestId container_id(registration->VmType(), registration->VmName(),
                                 registration->ContainerName());

  if (crostini_manager->IsUncleanStartup()) {
    VLOG(1) << "Unclean startup for " << container_id
            << " - showing recovery view";
    // Prompt for user-restart.
    return ShowCrostiniRecoveryView(
        profile, crostini::CrostiniUISurface::kAppList, app_id, display_id,
        args, std::move(callback));
  }

  if (crostini_manager->GetCrostiniDialogStatus(DialogType::UPGRADER)) {
    // Reshow the existing dialog.
    ash::CrostiniUpgraderDialog::Reshow();
    VLOG(1) << "Reshowing upgrade dialog";
    std::move(callback).Run(
        false, "LaunchCrostiniApp called while upgrade dialog showing");
    return;
  }

  LaunchCrostiniAppImpl(profile, app_id, std::move(*registration), container_id,
                        display_id, args, std::move(callback));
}

void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id,
                       const std::vector<guest_os::LaunchArg>& args,
                       CrostiniSuccessCallback callback) {
  LaunchCrostiniAppWithIntent(profile, app_id, display_id, nullptr, args,
                              std::move(callback));
}

std::vector<vm_tools::cicerone::ContainerFeature> GetContainerFeatures() {
  std::vector<vm_tools::cicerone::ContainerFeature> result;

  // TODO: b/303743348 - Update garcon to set this env var by default and
  // deprecate this feature.
  result.push_back(
      vm_tools::cicerone::ContainerFeature::ENABLE_GTK3_IME_SUPPORT);

  if (base::FeatureList::IsEnabled(ash::features::kCrostiniQtImeSupport)) {
    result.push_back(
        vm_tools::cicerone::ContainerFeature::ENABLE_QT_IME_SUPPORT);
  }
  if (base::FeatureList::IsEnabled(
          ash::features::kCrostiniVirtualKeyboardSupport)) {
    result.push_back(
        vm_tools::cicerone::ContainerFeature::ENABLE_VIRTUAL_KEYBOARD_SUPPORT);
  }
  return result;
}

std::string CryptohomeIdForProfile(Profile* profile) {
  std::string id = ash::ProfileHelper::GetUserIdHashFromProfile(profile);
  // Empty id means we're running in a test.
  return id.empty() ? "test" : id;
}

std::string DefaultContainerUserNameForProfile(Profile* profile) {
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user) {
    return kCrostiniDefaultUsername;
  }
  std::string username = user->GetAccountName(/*use_display_email=*/false);

  // For gmail accounts, dots are already stripped away in the canonical
  // username. But for other accounts (e.g. managedchrome), we need to do this
  // manually.
  std::string::size_type index;
  while ((index = username.find('.')) != std::string::npos) {
    username.erase(index, 1);
  }

  return username;
}

base::FilePath ContainerChromeOSBaseDirectory() {
  return base::FilePath("/mnt/chromeos");
}

void AddNewLxdContainerToPrefs(Profile* profile,
                               const guest_os::GuestId& container_id) {
  base::Value::Dict properties;
  properties.Set(guest_os::prefs::kContainerOsVersionKey,
                 static_cast<int>(ContainerOsVersion::kUnknown));
  properties.Set(guest_os::prefs::kContainerOsPrettyNameKey, "");
  guest_os::AddContainerToPrefs(profile, container_id, std::move(properties));
}

void RemoveLxdContainerFromPrefs(Profile* profile,
                                 const guest_os::GuestId& container_id) {
  guest_os::RemoveContainerFromPrefs(profile, container_id);
  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile)
      ->ClearApplicationList(guest_os::VmType::TERMINA, container_id.vm_name,
                             container_id.container_name);
  guest_os::GuestOsMimeTypesServiceFactory::GetForProfile(profile)
      ->ClearMimeTypes(container_id.vm_name, container_id.container_name);
}

SkColor GetContainerBadgeColor(Profile* profile,
                               const guest_os::GuestId& container_id) {
  const base::Value* badge_color_value = GetContainerPrefValue(
      profile, container_id, guest_os::prefs::kContainerColorKey);
  if (badge_color_value) {
    return badge_color_value->GetIfInt().value_or(SK_ColorTRANSPARENT);
  } else {
    return SK_ColorTRANSPARENT;
  }
}

void SetContainerBadgeColor(Profile* profile,
                            const guest_os::GuestId& container_id,
                            SkColor badge_color) {
  guest_os::UpdateContainerPref(profile, container_id,
                                guest_os::prefs::kContainerColorKey,
                                base::Value(static_cast<int>(badge_color)));

  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile)
      ->ContainerBadgeColorChanged(container_id);
}

bool IsContainerVersionExpired(Profile* profile,
                               const guest_os::GuestId& container_id) {
  auto* value = GetContainerPrefValue(profile, container_id,
                                      guest_os::prefs::kContainerOsVersionKey);
  if (!value) {
    return false;
  }

  auto version = static_cast<ContainerOsVersion>(value->GetInt());
  return version == ContainerOsVersion::kDebianStretch ||
         version == ContainerOsVersion::kDebianBuster;
}

std::u16string GetTimeRemainingMessage(base::TimeTicks start, int percent) {
  // Only estimate once we've spent at least 3 seconds OR gotten 10% of the way
  // through.
  constexpr base::TimeDelta kMinTimeForEstimate = base::Seconds(3);
  constexpr base::TimeDelta kTimeDeltaZero = base::Seconds(0);
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

const guest_os::GuestId& DefaultContainerId() {
  static const base::NoDestructor<guest_os::GuestId> container_id(
      kCrostiniDefaultVmType, kCrostiniDefaultVmName,
      kCrostiniDefaultContainerName);
  return *container_id;
}

bool IsCrostiniWindow(const aura::Window* window) {
  // TODO(crbug/1158644): Non-Crostini apps (borealis, ...) have also been
  // identifying as Crostini. For now they're less common, and as they become
  // more productionised they get their own app type (e.g. lacros), but at some
  // point we'll want to untangle these different types to e.g. avoid double
  // counting in usage metrics.
  return window->GetProperty(chromeos::kAppTypeKey) ==
         chromeos::AppType::CROSTINI_APP;
}

void RecordAppLaunchHistogram(CrostiniAppLaunchAppType app_type) {
  base::UmaHistogramEnumeration(kCrostiniAppLaunchHistogram, app_type);
}

void RecordAppLaunchResultHistogram(CrostiniAppLaunchAppType type,
                                    crostini::CrostiniResult reason) {
  // We record one histogram for everything, so we have data continuity as
  // that's the metric we had first, and we also break results down by launch
  // type.
  base::UmaHistogramEnumeration(kCrostiniAppLaunchResultHistogram, reason);
  switch (type) {
    case CrostiniAppLaunchAppType::kTerminal:
      base::UmaHistogramEnumeration(kCrostiniAppLaunchResultHistogramTerminal,
                                    reason);
      break;
    case CrostiniAppLaunchAppType::kRegisteredApp:
      base::UmaHistogramEnumeration(kCrostiniAppLaunchResultHistogramRegistered,
                                    reason);
      break;
    case CrostiniAppLaunchAppType::kUnknownApp:
      base::UmaHistogramEnumeration(kCrostiniAppLaunchResultHistogramUnknown,
                                    reason);
      break;
  }
}

bool ShouldStopVm(Profile* profile, const guest_os::GuestId& container_id) {
  for (const auto& container :
       guest_os::GetContainers(profile, kCrostiniDefaultVmType)) {
    if (container.container_name != container_id.container_name &&
        container.vm_name == container_id.vm_name) {
      if (guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile)
              ->IsRunning(container)) {
        return false;
      }
    }
  }
  return true;
}

std::string FormatForUi(guest_os::GuestId guest_id) {
  if (guest_id.vm_name == kCrostiniDefaultVmName) {
    return guest_id.container_name;
  }
  return base::StrCat({guest_id.vm_name, ":", guest_id.container_name});
}

}  // namespace crostini
