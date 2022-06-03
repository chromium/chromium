// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNGRADE_DOWNGRADE_MANAGER_H_
#define CHROME_BROWSER_DOWNGRADE_DOWNGRADE_MANAGER_H_

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace downgrade {

// An encapsulation of processing relating to the handling of browser launches
// where the User Data directory was last written by a higher version of the
// browser (a "downgrade"). It can detect if downgrade processing is needed,
// drop a breadcrumb for future launches indicating the current browser version,
// delete leftover state from a previous downgrade, and perform processing on
// state deposited on the device by the browser (e.g., the User Data directory)
// following a downgrade.
class DowngradeManager {
 public:
  DowngradeManager() = default;
  DowngradeManager(const DowngradeManager&) = delete;
  DowngradeManager& operator=(const DowngradeManager&) = delete;

  // Inspects the contents of |user_data_dir| to determine whether a downgrade
  // or an upgrade has happened since the last launch. Takes a Snapshot in case
  // of upgrade, and sets the appropriate |type_| in case of downgrade. Returns
  // |true| if the data from |user_data_dir| requires migration processing,
  // |false| if it is usable by the current version. Note: this must be called
  // within the protection of the process singleton.
  bool PrepareUserDataDirectoryForCurrentVersion(
      const base::FilePath& user_data_dir);

  // Writes the current version number into the "Last Version" file in
  // |user_data_dir|.
  void UpdateLastVersion(const base::FilePath& user_data_dir);

  // Schedules a search for the removal of any directories moved aside by
  // PrepareUserDataDirectoryForCurrentVersion or ProcessDowngrade. This
  // operation is idempotent, and may be safely called when no such directories
  // exist.
  void DeleteMovedUserDataSoon(const base::FilePath& user_data_dir);

  // Process a previously-detected downgrade of |user_data_dir|. This must be
  // called late in shutdown while the process singleton is still held.
  void ProcessDowngrade(const base::FilePath& user_data_dir);

  static void EnableSnapshotsForTesting(bool enable);

 private:
  enum class Type {
    kNone = 0,                // Same version or upgrade.
    kAdministrativeWipe = 1,  // Admin-driven downgrade with no snapshot.
    kUnsupported = 2,         // Unsupported downgrade with no data processing.
    kSnapshotRestore = 3,     // Downgrade with snapshot restoration.
    kMinorDowngrade = 4,      // Minor version downgrade; no data processing.
    kMaxValue = kMinorDowngrade
  };

  static Type GetDowngradeType(const base::FilePath& user_data_dir,
                               const base::Version& current_version,
                               const base::Version& last_version);

  static Type GetDowngradeTypeWithSnapshot(const base::FilePath& user_data_dir,
                                           const base::Version& current_version,
                                           const base::Version& last_version);

  Type type_ = Type::kNone;
};

}  // namespace downgrade

#endif  // CHROME_BROWSER_DOWNGRADE_DOWNGRADE_MANAGER_H_
