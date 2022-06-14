// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_util.h"

#include <utility>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_crostini_tracker.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_item_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_dialog.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace crostini {

const char kCrostiniImageAliasPattern[] = "debian/%s";
const char kCrostiniContainerDefaultVersion[] = "bullseye";
const char kCrostiniContainerFlag[] = "crostini-container-install-version";

const char kCrostiniDefaultVmName[] = "termina";
const char kCrostiniDefaultContainerName[] = "penguin";
const char kCrostiniDefaultUsername[] = "emperor";
const char kCrostiniDefaultImageServerUrl[] =
    "https://storage.googleapis.com/cros-containers/%d";
const char kCrostiniDlcName[] = "termina-dlc";

const base::FilePath::CharType kHomeDirectory[] = FILE_PATH_LITERAL("/home");

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
    int64_t display_id,
    const std::vector<std::string>& args,
    crostini::CrostiniSuccessCallback callback,
    bool success,
    const std::string& failure_reason) {
  if (!success) {
    return OnLaunchFailed(
        app_id, std::move(callback),
        "failed to share paths to launch " + app_id + ":" + failure_reason,
        CrostiniResult::SHARE_PATHS_FAILED);
  }
  const crostini::ContainerId container_id(registration.VmName(),
                                           registration.ContainerName());
  crostini::CrostiniManager::GetForProfile(profile)->LaunchContainerApplication(
      container_id, registration.DesktopFileId(), args, registration.IsScaled(),
      base::BindOnce(OnApplicationLaunched, app_id, std::move(callback),
                     crostini::CrostiniResult::UNKNOWN_ERROR));
}

void LaunchApplication(
    Profile* profile,
    const std::string& app_id,
    guest_os::GuestOsRegistryService::Registration registration,
    int64_t display_id,
    const std::vector<LaunchArg>& args,
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

  auto* share_path = guest_os::GuestOsSharePath::GetForProfile(profile);
  const auto vm_name = registration.VmName();

  // Share any paths not in crostini.  The user will see the spinner while this
  // is happening.
  std::vector<base::FilePath> paths_to_share;
  std::vector<std::string> launch_args;
  launch_args.reserve(args.size());
  for (const auto& arg : args) {
    if (absl::holds_alternative<std::string>(arg)) {
      launch_args.push_back(absl::get<std::string>(arg));
      continue;
    }
    const storage::FileSystemURL& url = absl::get<storage::FileSystemURL>(arg);
    base::FilePath path;
    if (!file_manager::util::ConvertFileSystemURLToPathInsideCrostini(
            profile, url, &path)) {
      return OnLaunchFailed(
          app_id, std::move(callback),
          "Cannot share file with crostini: " + url.DebugString(),
          CrostiniResult::SHARE_PATHS_FAILED);
    }
    if (url.mount_filesystem_id() !=
            file_manager::util::GetCrostiniMountPointName(profile) &&
        !share_path->IsPathShared(vm_name, url.path())) {
      paths_to_share.push_back(url.path());
    }
    launch_args.push_back(path.value());
  }

  share_path->SharePaths(
      vm_name, std::move(paths_to_share), /*persist=*/false,
      base::BindOnce(OnSharePathForLaunchApplication, profile, app_id,
                     std::move(registration), display_id,
                     std::move(launch_args), std::move(callback)));
}

}  // namespace

ContainerId::ContainerId(std::string vm_name,
                         std::string container_name) noexcept
    : vm_name(std::move(vm_name)), container_name(std::move(container_name)) {}

ContainerId::ContainerId(const base::Value& value) noexcept {
  const base::Value::Dict* dict = value.GetIfDict();
  const std::string* vm = nullptr;
  const std::string* container = nullptr;
  if (dict != nullptr) {
    vm = dict->FindString(prefs::kVmKey);
    container = dict->FindString(prefs::kContainerKey);
  }
  vm_name = vm ? *vm : "";
  container_name = container ? *container : "";
}

base::flat_map<std::string, std::string> ContainerId::ToMap() const {
  base::flat_map<std::string, std::string> extras;
  extras[prefs::kVmKey] = vm_name;
  extras[prefs::kContainerKey] = container_name;
  return extras;
}

base::Value::Dict ContainerId::ToDictValue() const {
  base::Value::Dict dict;
  dict.Set(prefs::kVmKey, vm_name);
  dict.Set(prefs::kContainerKey, container_name);
  return dict;
}

bool operator<(const ContainerId& lhs, const ContainerId& rhs) noexcept {
  const auto result = lhs.vm_name.compare(rhs.vm_name);
  return result < 0 || (result == 0 && lhs.container_name < rhs.container_name);
}

bool operator==(const ContainerId& lhs, const ContainerId& rhs) noexcept {
  return lhs.vm_name == rhs.vm_name && lhs.container_name == rhs.container_name;
}

std::ostream& operator<<(std::ostream& ostream,
                         const ContainerId& container_id) {
  return ostream << "(vm: \"" << container_id.vm_name << "\" container: \""
                 << container_id.container_name << "\")";
}

ContainerId ContainerId::GetDefault() {
  return ContainerId(kCrostiniDefaultVmName, kCrostiniDefaultContainerName);
}

bool IsUninstallable(Profile* profile, const std::string& app_id) {
  if (!CrostiniFeatures::Get()->IsEnabled(profile)) {
    return false;
  }
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  absl::optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);
  if (registration)
    return registration->CanUninstall();
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
             ->IsContainerUpgradeable(ContainerId(
                 kCrostiniDefaultVmName, kCrostiniDefaultContainerName));
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
    const ContainerId container_id,
    int64_t display_id,
    const std::vector<LaunchArg>& args,
    CrostiniSuccessCallback callback) {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile);
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kRegisteredApp);

  // Update the last launched time and Termina version.
  registry_service->AppLaunched(app_id);
  crostini_manager->UpdateLaunchMetricsForEnterpriseReporting();

  auto restart_id = crostini_manager->RestartCrostini(
      container_id,
      base::BindOnce(
          [](Profile* profile, const std::string& app_id,
             guest_os::GuestOsRegistryService::Registration registration,
             int64_t display_id, const std::vector<LaunchArg> args,
             crostini::CrostiniSuccessCallback callback,
             crostini::CrostiniResult result) {
            if (result != crostini::CrostiniResult::SUCCESS) {
              OnLaunchFailed(app_id, std::move(callback),
                             base::StringPrintf(
                                 "crostini restart to launch app %s failed: %d",
                                 app_id.c_str(), result),
                             result);
              return;
            }

            LaunchApplication(profile, app_id, std::move(registration),
                              display_id, args, std::move(callback));
          },
          profile, app_id, std::move(registration), display_id, args,
          std::move(callback)));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&AddSpinner, restart_id, app_id, profile),
      base::Milliseconds(kDelayBeforeSpinnerMs));
}

void LaunchCrostiniAppWithIntent(Profile* profile,
                                 const std::string& app_id,
                                 int64_t display_id,
                                 apps::mojom::IntentPtr intent,
                                 const std::vector<LaunchArg>& args,
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
  absl::optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);

  if (!registration) {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kUnknownApp);
    RecordAppLaunchResultHistogram(CrostiniAppLaunchAppType::kUnknownApp,
                                   CrostiniResult::UNREGISTERED_APPLICATION);
    return std::move(callback).Run(
        false, "LaunchCrostiniApp called with an unknown app_id: " + app_id);
  }
  ContainerId container_id(registration->VmName(),
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
    chromeos::CrostiniUpgraderDialog::Reshow();
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
                       const std::vector<LaunchArg>& args,
                       CrostiniSuccessCallback callback) {
  LaunchCrostiniAppWithIntent(profile, app_id, display_id, nullptr, args,
                              std::move(callback));
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

namespace {

bool MatchContainerDict(const base::Value& dict,
                        const ContainerId& container_id) {
  const std::string* vm_name = dict.FindStringKey(prefs::kVmKey);
  const std::string* container_name = dict.FindStringKey(prefs::kContainerKey);
  return (vm_name && *vm_name == container_id.vm_name) &&
         (container_name && *container_name == container_id.container_name);
}

}  // namespace

void RemoveDuplicateContainerEntries(PrefService* prefs) {
  ListPrefUpdate updater(prefs, crostini::prefs::kCrostiniContainers);

  std::set<ContainerId> seen_containers;
  auto& containers = updater->GetList();
  for (auto it = containers.begin(); it != containers.end();) {
    ContainerId containerId(*it);
    if (seen_containers.find(containerId) == seen_containers.end()) {
      seen_containers.insert(containerId);
      it++;
    } else {
      it = containers.erase(it);
    }
  }
}

std::vector<ContainerId> GetContainers(Profile* profile) {
  std::vector<ContainerId> result;
  const base::Value::List& container_list =
      profile->GetPrefs()
          ->GetList(crostini::prefs::kCrostiniContainers)
          ->GetList();
  for (const auto& container : container_list) {
    crostini::ContainerId id(container);
    if (!id.vm_name.empty() && !id.container_name.empty()) {
      result.push_back(std::move(id));
    }
  }
  return result;
}

void AddNewLxdContainerToPrefs(Profile* profile,
                               const ContainerId& container_id) {
  ListPrefUpdate updater(profile->GetPrefs(),
                         crostini::prefs::kCrostiniContainers);
  auto it = std::find_if(
      updater->GetListDeprecated().begin(), updater->GetListDeprecated().end(),
      [&](const auto& dict) { return MatchContainerDict(dict, container_id); });
  if (it != updater->GetListDeprecated().end()) {
    return;
  }

  base::Value new_container(base::Value::Type::DICTIONARY);
  new_container.SetKey(prefs::kVmKey, base::Value(container_id.vm_name));
  new_container.SetKey(prefs::kContainerKey,
                       base::Value(container_id.container_name));
  new_container.SetIntKey(prefs::kContainerOsVersionKey,
                          static_cast<int>(ContainerOsVersion::kUnknown));
  new_container.SetStringKey(prefs::kContainerOsPrettyNameKey, "");
  updater->Append(std::move(new_container));
}

void RemoveLxdContainerFromPrefs(Profile* profile,
                                 const ContainerId& container_id) {
  auto* pref_service = profile->GetPrefs();
  ListPrefUpdate updater(pref_service, crostini::prefs::kCrostiniContainers);
  updater->EraseListIter(
      std::find_if(updater->GetListDeprecated().begin(),
                   updater->GetListDeprecated().end(), [&](const auto& dict) {
                     return MatchContainerDict(dict, container_id);
                   }));

  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile)
      ->ClearApplicationList(guest_os::GuestOsRegistryService::VmType::
                                 ApplicationList_VmType_TERMINA,
                             container_id.vm_name, container_id.container_name);
  guest_os::GuestOsMimeTypesServiceFactory::GetForProfile(profile)
      ->ClearMimeTypes(container_id.vm_name, container_id.container_name);
}

const base::Value* GetContainerPrefValue(Profile* profile,
                                         const ContainerId& container_id,
                                         const std::string& key) {
  const base::Value* containers =
      profile->GetPrefs()->GetList(crostini::prefs::kCrostiniContainers);
  if (!containers) {
    return nullptr;
  }
  for (const auto& dict : containers->GetListDeprecated()) {
    if (MatchContainerDict(dict, container_id))
      return dict.FindKey(key);
  }
  return nullptr;
}

void UpdateContainerPref(Profile* profile,
                         const ContainerId& container_id,
                         const std::string& key,
                         base::Value value) {
  ListPrefUpdate updater(profile->GetPrefs(),
                         crostini::prefs::kCrostiniContainers);
  auto it = std::find_if(
      updater->GetListDeprecated().begin(), updater->GetListDeprecated().end(),
      [&](const auto& dict) { return MatchContainerDict(dict, container_id); });
  if (it != updater->GetListDeprecated().end()) {
    it->SetKey(key, std::move(value));
  }
}

SkColor GetContainerBadgeColor(Profile* profile,
                               const ContainerId& container_id) {
  const base::Value* badge_color_value =
      GetContainerPrefValue(profile, container_id, prefs::kContainerColorKey);
  if (badge_color_value) {
    return badge_color_value->GetIfInt().value_or(SK_ColorTRANSPARENT);
  } else {
    return SK_ColorTRANSPARENT;
  }
}

void SetContainerBadgeColor(Profile* profile,
                            const ContainerId& container_id,
                            SkColor badge_color) {
  UpdateContainerPref(profile, container_id, prefs::kContainerColorKey,
                      base::Value(static_cast<int>(badge_color)));

  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile)
      ->ContainerBadgeColorChanged(container_id);
}

bool IsContainerVersionExpired(Profile* profile,
                               const ContainerId& container_id) {
  auto* value = GetContainerPrefValue(profile, container_id,
                                      prefs::kContainerOsVersionKey);
  if (!value)
    return false;

  auto version = static_cast<ContainerOsVersion>(value->GetInt());
  return version == ContainerOsVersion::kDebianStretch;
}

bool ShouldWarnAboutExpiredVersion(Profile* profile,
                                   const ContainerId& container_id) {
  if (!CrostiniFeatures::Get()->IsContainerUpgradeUIAllowed(profile)) {
    return false;
  }
  if (container_id != ContainerId::GetDefault()) {
    return false;
  }
  // If the warning dialog is already open we can add more callbacks to it, but
  // if we've moved to the upgrade dialog proper we should run them now as they
  // may be part of the upgrade process.
  if (chromeos::SystemWebDialogDelegate::FindInstance(
          GURL{chrome::kChromeUICrostiniUpgraderUrl}.spec())) {
    return false;
  }
  return IsContainerVersionExpired(profile, container_id);
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

const ContainerId& DefaultContainerId() {
  static const base::NoDestructor<ContainerId> container_id(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName);
  return *container_id;
}

bool IsCrostiniWindow(const aura::Window* window) {
  // TODO(crbug/1158644): Non-Crostini apps (borealis, ...) have also been
  // identifying as Crostini. For now they're less common, and as they become
  // more productionised they get their own app type (e.g. lacros), but at some
  // point we'll want to untangle these different types to e.g. avoid double
  // counting in usage metrics.
  return window->GetProperty(aura::client::kAppType) ==
         static_cast<int>(ash::AppType::CROSTINI_APP);
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

bool ShouldStopVm(Profile* profile, const ContainerId& container_id) {
  bool is_last_container = true;
  base::Value::ConstListView containers =
      profile->GetPrefs()
          ->GetList(prefs::kCrostiniContainers)
          ->GetListDeprecated();
  for (const auto& dict : containers) {
    ContainerId container(dict);
    if (container.container_name != container_id.container_name &&
        container.vm_name == container_id.vm_name) {
      if (CrostiniManager::GetForProfile(profile)->GetContainerInfo(
              container)) {
        is_last_container = false;
        break;
      }
    }
  }
  return is_last_container;
}

}  // namespace crostini
