// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DRIVE_FILE_SYSTEM_UTIL_H_
#define CHROME_BROWSER_ASH_DRIVE_FILE_SYSTEM_UTIL_H_

#include <ostream>

#include "base/files/file_path.h"

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

// Drive availability for a given profile.
enum class DriveAvailability {
  kAvailable,
  kNotAvailableWhenDisableDrivePreferenceSet,
  kNotAvailableForAccountType,
  kNotAvailableForUninitialisedLoginState,
  kNotAvailableInIncognito,
  kNotAvailableForTestImage,
};

// Returns the Drive availability for a given profile. Checks if Drive is
// enabled or if Drive is available for the given profile.
DriveAvailability CheckDriveEnabledAndDriveAvailabilityForProfile(
    const Profile* const profile);

// Returns true if the bulk-pinning feature should be available and visible in
// the given Profile. Several conditions need to be met for the bulk-pinning
// feature to be available. This does not indicate whether the bulk-pinning
// feature has been activated (turned on) by the user. It merely indicates
// whether the bulk-pinning feature is available and can be turned on by the
// user if they choose to.
[[nodiscard]] bool IsDriveFsBulkPinningAvailable(const Profile* profile);
[[nodiscard]] bool IsDriveFsBulkPinningAvailable();
[[nodiscard]] bool IsOobeDrivePinningAvailable(const Profile* profile);
[[nodiscard]] bool IsOobeDrivePinningAvailable();
[[nodiscard]] bool IsOobeDrivePinningScreenEnabled();

// Returns true if the mirror sync feature should be available and visible in
// the given Profile. This does not indicate whether the mirror sync
// feature has been activated (turned on) by the user. It merely indicates
// whether the mirror sync feature is available and can be turned on by the
// user if they choose to.
[[nodiscard]] bool IsDriveFsMirrorSyncAvailable(const Profile* profile);

// Connection status to Drive.
enum class ConnectionStatus {
  // Disconnected because Drive service is unavailable for this account (either
  // disabled by a flag or the account has no Google account (e.g., guests)).
  kNoService,
  // Disconnected because no network is available.
  kNoNetwork,
  // Disconnected because authentication is not ready.
  kNotReady,
  // Connected by metered network (eg cellular network, or metered WiFi.)
  // Background sync is disabled.
  kMetered,
  // Connected without limitation (WiFi, Ethernet, or cellular with the
  // disable-sync preference turned off.)
  kConnected,
};

std::ostream& operator<<(std::ostream& out, ConnectionStatus status);

// Sets the Drive connection status for testing purposes.
void SetDriveConnectionStatusForTesting(ConnectionStatus status);

// Returns the Drive connection status for the `profile`. Also returns the
// device's online state in `is_online`. This could be different from the
// connection status if drivefs is not running for some reason.
ConnectionStatus GetDriveConnectionStatus(Profile* profile,
                                          bool* is_online = nullptr);

// Returns true if the supplied mime type is of a pinnable type. This indicates
// the file can be made available offline.
bool IsPinnableGDocMimeType(const std::string& mime_type);

// Computes the total content cache size (minus the chunks.db* metadata files).
int64_t ComputeDriveFsContentCacheSize(const base::FilePath& path);

}  // namespace util
}  // namespace drive

#endif  // CHROME_BROWSER_ASH_DRIVE_FILE_SYSTEM_UTIL_H_
