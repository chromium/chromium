// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/environment_provider.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/web_applications/preinstalled_web_app_config_utils.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/policy_namespace.mojom.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "crypto/nss_util_internal.h"

namespace crosapi {

// static
EnvironmentProvider* EnvironmentProvider::Get() {
  static base::NoDestructor<EnvironmentProvider> provider;
  return provider.get();
}

EnvironmentProvider::EnvironmentProvider() = default;
EnvironmentProvider::~EnvironmentProvider() = default;

mojom::DefaultPathsPtr EnvironmentProvider::GetDefaultPaths() {
  mojom::DefaultPathsPtr default_paths = mojom::DefaultPaths::New();
  // The default paths belong to ash's primary user profile. Lacros does not
  // support multi-signin.
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);

  default_paths->user_nss_database =
      crypto::GetSoftwareNSSDBPath(profile->GetPath());

  if (base::SysInfo::IsRunningOnChromeOS()) {
    // Typically /home/chronos/u-<hash>/MyFiles.
    default_paths->documents =
        file_manager::util::GetMyFilesFolderForProfile(profile);
    // Typically /home/chronos/u-<hash>/MyFiles/Downloads.
    default_paths->downloads =
        file_manager::util::GetDownloadsFolderForProfile(profile);
    auto* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile);
    if (integration_service && integration_service->is_enabled() &&
        integration_service->IsMounted()) {
      default_paths->drivefs = integration_service->GetMountPointPath();
    }
    if (ash::cloud_upload::IsODFSMounted(profile)) {
      default_paths->onedrive = ash::cloud_upload::GetODFSFuseboxMount(profile);
    }
    default_paths->android_files =
        base::FilePath(file_manager::util::GetAndroidFilesPath());
    default_paths->linux_files =
        file_manager::util::GetCrostiniMountDirectory(profile);
    base::FilePath ash_resources;
    if (base::PathService::Get(base::DIR_ASSETS, &ash_resources))
      default_paths->ash_resources = ash_resources;
  } else {
    // On developer linux workstations the above functions do path mangling to
    // support multi-signin which gets undone later in ash-specific code. This
    // is awkward for Lacros development, so just provide some defaults.
    base::FilePath home = base::GetHomeDir();
    default_paths->documents = home.Append("Documents");
    default_paths->downloads = home.Append("Downloads");
    default_paths->drivefs = home.Append("Drive");
    default_paths->onedrive = home.Append("fsp");
    default_paths->android_files = home.Append("Android");
    default_paths->linux_files = home.Append("Crostini");
    default_paths->ash_resources = home.Append("Ash");
  }

  // CrosDisksClient already has a convention for its removable media directory
  // when running on Linux workstations.
  default_paths->removable_media =
      ash::CrosDisksClient::GetRemovableDiskMountPoint();

  // Ash expects to find shared files in the share cache.
  default_paths->share_cache =
      file_manager::util::GetShareCacheFilePath(profile);

  default_paths->preinstalled_web_app_config =
      web_app::GetPreinstalledWebAppConfigDirFromCommandLine(profile);
  default_paths->preinstalled_web_app_extra_config =
      web_app::GetPreinstalledWebAppExtraConfigDirFromCommandLine(profile);

  return default_paths;
}

}  // namespace crosapi
