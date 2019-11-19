// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system_util.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/profiles/profile_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chromeos/constants/chromeos_constants.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/escape.h"
#include "services/network/public/cpp/network_connection_tracker.h"

using content::BrowserThread;

namespace drive {
namespace util {

DriveIntegrationService* GetIntegrationServiceByProfile(Profile* profile) {
  DriveIntegrationService* service =
      DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!service || !service->IsMounted())
    return nullptr;
  return service;
}

bool IsUnderDriveMountPoint(const base::FilePath& path) {
  return !ExtractDrivePath(path).empty();
}

base::FilePath ExtractDrivePath(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);
  if (components.size() < 3)
    return base::FilePath();
  if (components[0] != FILE_PATH_LITERAL("/"))
    return base::FilePath();
  if (components[1] != FILE_PATH_LITERAL("special"))
    return base::FilePath();
  static const base::FilePath::CharType kPrefix[] = FILE_PATH_LITERAL("drive");
  if (components[2].compare(0, base::size(kPrefix) - 1, kPrefix) != 0)
    return base::FilePath();

  base::FilePath drive_path = GetDriveGrandRootPath();
  for (size_t i = 3; i < components.size(); ++i)
    drive_path = drive_path.Append(components[i]);
  return drive_path;
}

base::FilePath GetCacheRootPath(Profile* profile) {
  base::FilePath cache_base_path;
  chrome::GetUserCacheDirectory(profile->GetPath(), &cache_base_path);
  base::FilePath cache_root_path =
      cache_base_path.Append(chromeos::kDriveCacheDirname);
  static const base::FilePath::CharType kFileCacheVersionDir[] =
      FILE_PATH_LITERAL("v1");
  return cache_root_path.Append(kFileCacheVersionDir);
}

bool IsDriveEnabledForProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!chromeos::IsProfileAssociatedWithGaiaAccount(profile))
    return false;

  // Disable Drive if preference is set. This can happen with commandline flag
  // --disable-drive or enterprise policy, or with user settings.
  if (profile->GetPrefs()->GetBoolean(prefs::kDisableDrive))
    return false;

  // Disable drive if sync is disabled by command line flag. Outside tests, this
  // only occurs in cases already handled by the gaia account check above.
  if (!switches::IsSyncAllowedByFlag())
    return false;

  return true;
}

ConnectionStatusType GetDriveConnectionStatus(Profile* profile) {
  auto* drive_integration_service = GetIntegrationServiceByProfile(profile);
  if (!drive_integration_service)
    return DRIVE_DISCONNECTED_NOSERVICE;
  auto* network_connection_tracker = content::GetNetworkConnectionTracker();
  if (network_connection_tracker->IsOffline())
    return DRIVE_DISCONNECTED_NONETWORK;

  auto connection_type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  network_connection_tracker->GetConnectionType(&connection_type,
                                                base::DoNothing());
  const bool is_connection_cellular =
      network::NetworkConnectionTracker::IsConnectionCellular(connection_type);
  const bool disable_sync_over_celluar =
      profile->GetPrefs()->GetBoolean(prefs::kDisableDriveOverCellular);

  if (is_connection_cellular && disable_sync_over_celluar)
    return DRIVE_CONNECTED_METERED;
  return DRIVE_CONNECTED;
}

}  // namespace util
}  // namespace drive
