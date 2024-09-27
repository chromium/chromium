// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_files.h"

#include <utility>

#include "ash/public/cpp/shelf_model.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/app_window_base.h"
#include "chrome/browser/ui/ash/shelf/app_window_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace plugin_vm {

namespace {

void DirExistsResult(
    const base::FilePath& dir,
    bool result,
    base::OnceCallback<void(const base::FilePath&, bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(dir, result);
}

void EnsureDirExists(
    base::FilePath dir,
    base::OnceCallback<void(const base::FilePath&, bool)> callback) {
  base::File::Error error = base::File::FILE_OK;
  bool result = base::CreateDirectoryAndGetError(dir, &error);
  if (!result) {
    LOG(ERROR) << "Failed to create PluginVm shared dir " << dir.value() << ": "
               << base::File::ErrorToString(error);
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DirExistsResult, dir, result, std::move(callback)));
}

base::FilePath GetDefaultSharedDir(Profile* profile) {
  return file_manager::util::GetMyFilesFolderForProfile(profile).Append(
      kPluginVmName);
}

void FocusAllPluginVmWindows() {
  ash::ShelfModel* shelf_model =
      ChromeShelfController::instance()->shelf_model();
  DCHECK(shelf_model);
  AppWindowShelfItemController* item_controller =
      shelf_model->GetAppWindowShelfItemController(
          ash::ShelfID(kPluginVmShelfAppId));
  if (!item_controller) {
    return;
  }
  for (AppWindowBase* app_window : item_controller->windows()) {
    app_window->Activate();
  }
}

// LaunchPluginVmApp will run before this and try to start Plugin VM.
void LaunchPluginVmAppImpl(Profile* profile,
                           std::string app_id,
                           std::vector<std::string> file_paths,
                           LaunchPluginVmAppCallback callback,
                           bool plugin_vm_is_running) {
  if (!plugin_vm_is_running) {
    return std::move(callback).Run(LaunchPluginVmAppResult::FAILED,
                                   "Plugin VM could not be started");
  }

  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  auto registration = registry_service->GetRegistration(app_id);
  if (!registration) {
    return std::move(callback).Run(
        LaunchPluginVmAppResult::FAILED,
        "LaunchPluginVmApp called with an unknown app_id: " + app_id);
  }

  vm_tools::cicerone::LaunchContainerApplicationRequest request;
  request.set_owner_id(ash::ProfileHelper::GetUserIdHashFromProfile(profile));
  request.set_vm_name(registration->VmName());
  request.set_container_name(registration->ContainerName());
  request.set_desktop_file_id(registration->DesktopFileId());
  base::ranges::move(file_paths, google::protobuf::RepeatedFieldBackInserter(
                                     request.mutable_files()));

  ash::CiceroneClient::Get()->LaunchContainerApplication(
      std::move(request),
      base::BindOnce(
          [](const std::string& app_id, LaunchPluginVmAppCallback callback,
             std::optional<
                 vm_tools::cicerone::LaunchContainerApplicationResponse>
                 response) {
            if (!response || !response->success()) {
              LOG(ERROR) << "Failed to launch application. "
                         << (response ? response->failure_reason()
                                      : "Empty response.");
              std::move(callback).Run(LaunchPluginVmAppResult::FAILED,
                                      "Failed to launch " + app_id);
              return;
            }

            FocusAllPluginVmWindows();

            std::move(callback).Run(LaunchPluginVmAppResult::SUCCESS, "");
          },
          std::move(app_id), std::move(callback)));
}

}  // namespace

base::FilePath ChromeOSBaseDirectory() {
  // Forward slashes are converted to backslash during path conversion.
  return base::FilePath("//ChromeOS");
}

void EnsureDefaultSharedDirExists(
    Profile* profile,
    base::OnceCallback<void(const base::FilePath&, bool)> callback) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&EnsureDirExists, GetDefaultSharedDir(profile),
                     std::move(callback)));
}

void LaunchPluginVmApp(Profile* profile,
                       std::string app_id,
                       const std::vector<guest_os::LaunchArg>& args,
                       LaunchPluginVmAppCallback callback) {
  if (!plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile)) {
    return std::move(callback).Run(LaunchPluginVmAppResult::FAILED,
                                   "Plugin VM is not enabled for this profile");
  }

  auto* manager = PluginVmManagerFactory::GetForProfile(profile);

  if (!manager) {
    return std::move(callback).Run(LaunchPluginVmAppResult::FAILED,
                                   "Could not get PluginVmManager");
  }
  auto* share_path = guest_os::GuestOsSharePathFactory::GetForProfile(profile);
  base::FilePath vm_mount = ChromeOSBaseDirectory();

  std::vector<std::string> launch_args;
  launch_args.reserve(args.size());
  for (const auto& arg : args) {
    if (absl::holds_alternative<std::string>(arg)) {
      launch_args.push_back(absl::get<std::string>(arg));
      continue;
    }
    const storage::FileSystemURL& url = absl::get<storage::FileSystemURL>(arg);
    base::FilePath file_path;
    // Validate paths in MyFiles/PvmDefault, or are already shared, and convert.
    bool shared = GetDefaultSharedDir(profile).IsParent(url.path()) ||
                  share_path->IsPathShared(kPluginVmName, url.path());
    if (!shared ||
        !file_manager::util::ConvertFileSystemURLToPathInsideVM(
            profile, url, vm_mount, /*map_crostini_home=*/false, &file_path)) {
      return std::move(callback).Run(
          LaunchPluginVmAppResult::FAILED_DIRECTORY_NOT_SHARED,
          "Only files in shared dirs are supported. Got: " + url.DebugString());
    }
    // Convert slashes: '/' => '\'.
    std::string result;
    base::ReplaceChars(file_path.value(), "/", "\\", &result);
    launch_args.push_back(std::move(result));
  }

  manager->LaunchPluginVm(
      base::BindOnce(&LaunchPluginVmAppImpl, profile, std::move(app_id),
                     std::move(launch_args), std::move(callback)));
}

}  // namespace plugin_vm
