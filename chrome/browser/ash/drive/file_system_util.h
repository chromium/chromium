// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DRIVE_FILE_SYSTEM_UTIL_H_
#define CHROME_BROWSER_ASH_DRIVE_FILE_SYSTEM_UTIL_H_

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

// Gets the cache root path (i.e. <user_profile_dir>/GCache/v1) from the
// profile.
base::FilePath GetCacheRootPath(const Profile* profile);

// Returns true if Drive is available for the given Profile.
bool IsDriveAvailableForProfile(const Profile* profile);

// Returns true if Drive is currently enabled for the given Profile.
bool IsDriveEnabledForProfile(const Profile* profile);

// Returns true if the bulk-pinning feature should be available and visible in
// the given Profile. Several conditions need to be met for the bulk-pinning
// feature to be available. This does not indicate whether the bulk-pinning
// feature has been activated (turned on) by the user. It merely indicates
// whether the bulk-pinning feature is available and can be turned on by the
// user if they choose to.
bool IsDriveFsBulkPinningEnabled(const Profile* profile = nullptr);
bool IsOobeDrivePinningEnabled(const Profile* profile = nullptr);

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

// Returns true if the supplied mime type is of a pinnable type. This indicates
// the file can be made available offline.
bool IsPinnableGDocMimeType(const std::string& mime_type);

// Computes the total content cache size (minus the chunks.db* metadata files).
int64_t ComputeDriveFsContentCacheSize(
    const base::FilePath& content_cache_path);

}  // namespace util
}  // namespace drive

#endif  // CHROME_BROWSER_ASH_DRIVE_FILE_SYSTEM_UTIL_H_
