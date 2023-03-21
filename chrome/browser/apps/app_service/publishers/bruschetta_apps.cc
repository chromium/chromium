// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/bruschetta_apps.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/bruschetta/bruschetta_features.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_item_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "storage/browser/file_system/file_system_context.h"

namespace apps {

namespace {

void AddSpinner(const std::string& app_id) {
  if (auto* chrome_controller = ChromeShelfController::instance()) {
    chrome_controller->GetShelfSpinnerController()->AddSpinnerToShelf(
        app_id, std::make_unique<ShelfSpinnerItemController>(app_id));
  }
}

// It's safe to remove a non-existent spinner.
void RemoveSpinner(const std::string& app_id) {
  if (auto* chrome_controller = ChromeShelfController::instance()) {
    chrome_controller->GetShelfSpinnerController()->CloseSpinner(app_id);
  }
}

void OnLaunchFailed(const std::string& app_id,
                    LaunchCallback callback,
                    const std::string& reason) {
  LOG(ERROR) << "Failed to launch Bruschetta app " << app_id << ": " << reason;
  RemoveSpinner(app_id);
  std::move(callback).Run(ConvertBoolToLaunchResult(false));
}

void OnSharePathForLaunchApplication(
    Profile* profile,
    const std::string& app_id,
    guest_os::GuestOsRegistryService::Registration registration,
    const guest_os::GuestId container_id,
    const std::vector<std::string>& args,
    LaunchCallback callback,
    bool success,
    const std::string& failure_reason) {
  if (!success) {
    OnLaunchFailed(app_id, std::move(callback),
                   "Failed to share paths with Bruschetta: " + failure_reason);
    return;
  }
  // TODO(b/265601951): Factor this out of CrostiniManager.
  crostini::CrostiniManager::GetForProfile(profile)->LaunchContainerApplication(
      container_id, registration.DesktopFileId(), args, registration.IsScaled(),
      base::BindOnce(
          [](const std::string& app_id, LaunchCallback callback, bool success,
             const std::string& failure_reason) {
            if (!success) {
              OnLaunchFailed(app_id, std::move(callback), failure_reason);
              return;
            }
            RemoveSpinner(app_id);
            std::move(callback).Run(ConvertBoolToLaunchResult(success));
          },
          app_id, std::move(callback)));
}

void LaunchApplication(
    Profile* profile,
    const std::string& app_id,
    guest_os::GuestOsRegistryService::Registration registration,
    int64_t display_id,
    const std::vector<crostini::LaunchArg> args,
    LaunchCallback callback) {
  // TODO(b/265601951): Handle window permissions. Crostini uses
  // AppServiceAppWindowCrostiniTracker::OnAppLaunchRequested for this.

  const guest_os::GuestId container_id(registration.VmType(),
                                       registration.VmName(),
                                       registration.ContainerName());

  // TODO(b/265601951): Consider factoring SharePath code against
  // crostini_util.cc LaunchApplication.
  auto* share_path = guest_os::GuestOsSharePath::GetForProfile(profile);
  const std::string& vm_name = registration.VmName();
  auto vm_info =
      guest_os::GuestOsSessionTracker::GetForProfile(profile)->GetVmInfo(
          vm_name);
  if (!vm_info) {
    OnLaunchFailed(app_id, std::move(callback),
                   "Bruschetta VM not running: " + vm_name);
    return;
  }

  // Share any paths not in Bruschetta.
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
    if (!file_manager::util::ConvertFileSystemURLToPathInsideVM(
            profile, url, bruschetta::BruschettaChromeOSBaseDirectory(),
            /*map_crostini_home=*/false, &path)) {
      OnLaunchFailed(app_id, std::move(callback),
                     "Cannot share file with Bruschetta: " + url.DebugString());
      return;
    }
    if (url.mount_filesystem_id() !=
            file_manager::util::GetGuestOsMountPointName(profile,
                                                         container_id) &&
        !share_path->IsPathShared(vm_name, url.path())) {
      paths_to_share.push_back(url.path());
    }
    launch_args.push_back(path.value());
  }

  share_path->SharePaths(
      vm_name, vm_info->seneschal_server_handle(), std::move(paths_to_share),
      base::BindOnce(OnSharePathForLaunchApplication, profile, app_id,
                     std::move(registration), std::move(container_id),
                     std::move(launch_args), std::move(callback)));
}

}  // namespace

BruschettaApps::BruschettaApps(AppServiceProxy* proxy) : GuestOSApps(proxy) {}

bool BruschettaApps::CouldBeAllowed() const {
  return bruschetta::BruschettaFeatures::Get()->IsEnabled();
}

apps::AppType BruschettaApps::AppType() const {
  return AppType::kBruschetta;
}

guest_os::VmType BruschettaApps::VmType() const {
  return guest_os::VmType::BRUSCHETTA;
}

void BruschettaApps::LoadIcon(const std::string& app_id,
                              const IconKey& icon_key,
                              IconType icon_type,
                              int32_t size_hint_in_dip,
                              bool allow_placeholder_icon,
                              apps::LoadIconCallback callback) {
  // TODO(b/247636749): Consider creating IDR_LOGO_BRUSCHETTA_DEFAULT
  // to replace IconKey::kInvalidResourceId.
  registry()->LoadIcon(app_id, icon_key, icon_type, size_hint_in_dip,
                       allow_placeholder_icon, IconKey::kInvalidResourceId,
                       std::move(callback));
}

void BruschettaApps::Launch(const std::string& app_id,
                            int32_t event_flags,
                            LaunchSource launch_source,
                            WindowInfoPtr window_info) {
  LaunchAppWithIntent(app_id, event_flags, /*intent=*/nullptr, launch_source,
                      std::move(window_info), /*callback=*/base::DoNothing());
}

void BruschettaApps::LaunchAppWithIntent(const std::string& app_id,
                                         int32_t event_flags,
                                         IntentPtr intent,
                                         LaunchSource launch_source,
                                         WindowInfoPtr window_info,
                                         LaunchCallback callback) {
  // TODO(b/265601951): Consider factoring args code against
  // CrostiniApps::LaunchAppWithIntent.

  // Retrieve URLs from the files in the intent.
  std::vector<crostini::LaunchArg> args;
  if (intent && intent->files.size() > 0) {
    args.reserve(intent->files.size());
    storage::FileSystemContext* file_system_context =
        file_manager::util::GetFileManagerFileSystemContext(profile());
    for (auto& file : intent->files) {
      args.emplace_back(
          file_system_context->CrackURLInFirstPartyContext(file->url));
    }
  }

  const int64_t display_id =
      window_info ? window_info->display_id : display::kInvalidDisplayId;

  absl::optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry()->GetRegistration(app_id);
  if (!registration) {
    // TODO(b/247638226): RecordAppLaunchHistogram(kUnknown) to collect usage
    // stats for failed launches.
    OnLaunchFailed(app_id, std::move(callback),
                   "Unknown Bruschetta app_id: " + app_id);
    return;
  }

  // TODO(b/247638226): RecordAppLaunchHistogram(kRegisteredApp) to collect
  // usage stats for successful launches.

  // Update the last launched time.
  registry()->AppLaunched(app_id);

  // Start the bruschetta VM if necessary.
  const std::string& vm_name = registration->VmName();
  auto launcher =
      bruschetta::BruschettaService::GetForProfile(profile())->GetLauncher(
          vm_name);
  if (!launcher) {
    OnLaunchFailed(app_id, std::move(callback),
                   "Unknown Bruschetta VM name: " + vm_name);
    return;
  }
  AddSpinner(app_id);
  launcher->EnsureRunning(base::BindOnce(
      [](Profile* profile, const std::string& app_id,
         guest_os::GuestOsRegistryService::Registration registration,
         int64_t display_id, const std::vector<crostini::LaunchArg> args,
         const std::string& vm_name, LaunchCallback callback,
         bruschetta::BruschettaResult result) {
        if (result != bruschetta::BruschettaResult::kSuccess) {
          OnLaunchFailed(app_id, std::move(callback),
                         "Failed to start Bruschetta VM " + vm_name + ": " +
                             bruschetta::BruschettaResultString(result));
          return;
        }
        LaunchApplication(profile, app_id, std::move(registration), display_id,
                          std::move(args), std::move(callback));
      },
      profile(), app_id, std::move(registration.value()), display_id,
      std::move(args), vm_name, std::move(callback)));
}

void BruschettaApps::LaunchAppWithParams(AppLaunchParams&& params,
                                         LaunchCallback callback) {
  // TODO(b/265601951): Implement this.
}

void BruschettaApps::CreateAppOverrides(
    const guest_os::GuestOsRegistryService::Registration& registration,
    App* app) {
  // TODO(b/247638042): Implement IsUninstallable and use it here.
}

void BruschettaApps::GetMenuModel(
    const std::string& app_id,
    MenuType menu_type,
    int64_t display_id,
    base::OnceCallback<void(MenuItems)> callback) {
  MenuItems menu_items;

  if (menu_type == MenuType::kShelf) {
    AddCommandItem(ash::APP_CONTEXT_MENU_NEW_WINDOW, IDS_APP_LIST_NEW_WINDOW,
                   menu_items);
  }

  if (ShouldAddOpenItem(app_id, menu_type, profile())) {
    AddCommandItem(ash::LAUNCH_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC,
                   menu_items);
  }

  if (ShouldAddCloseItem(app_id, menu_type, profile())) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

}  // namespace apps
