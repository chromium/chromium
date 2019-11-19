// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UTIL_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "components/drive/file_errors.h"
#include "url/gurl.h"

class Profile;

namespace drive {

class DriveIntegrationService;

namespace util {

// Returns DriveIntegrationService instance, if Drive is enabled.
// Otherwise, nullptr.
DriveIntegrationService* GetIntegrationServiceByProfile(Profile*);

// Returns true if the given path is under the Drive mount point.
bool IsUnderDriveMountPoint(const base::FilePath& path);

// Extracts the Drive path from the given path located under the Drive mount
// point. Returns an empty path if |path| is not under the Drive mount point.
// Examples: ExtractDrivePath("/special/drive-xxx/foo.txt") => "drive/foo.txt"
base::FilePath ExtractDrivePath(const base::FilePath& path);

// Gets the cache root path (i.e. <user_profile_dir>/GCache/v1) from the
// profile.
base::FilePath GetCacheRootPath(Profile* profile);

// Returns true if Drive is enabled for the given Profile.
bool IsDriveEnabledForProfile(Profile* profile);

// Enum type for describing the current connection status to Drive.
enum ConnectionStatusType {
  // Disconnected because Drive service is unavailable for this account (either
  // disabled by a flag or the account has no Google account (e.g., guests)).
  DRIVE_DISCONNECTED_NOSERVICE,
  // Disconnected because no network is available.
  DRIVE_DISCONNECTED_NONETWORK,
  // Disconnected because authentication is not ready.
  DRIVE_DISCONNECTED_NOTREADY,
  // Connected by cellular network. Background sync is disabled.
  DRIVE_CONNECTED_METERED,
  // Connected without condition (WiFi, Ethernet, or cellular with the
  // disable-sync preference turned off.)
  DRIVE_CONNECTED,
};

// Returns the Drive connection status for the |profile|.
ConnectionStatusType GetDriveConnectionStatus(Profile* profile);

}  // namespace util
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UTIL_H_
