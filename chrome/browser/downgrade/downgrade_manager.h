// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNGRADE_DOWNGRADE_MANAGER_H_
#define CHROME_BROWSER_DOWNGRADE_DOWNGRADE_MANAGER_H_

#include "base/files/file_path.h"

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

  // Inspects the contents of |user_data_dir| to determine whether or not a
  // downgrade has happened since the last launch. Returns true if a downgrade
  // has been detected and that downgrade requires migration processing. Note:
  // this must be called within the protection of the process singleton.
  bool IsMigrationRequired(const base::FilePath& user_data_dir);

  // Writes the current version number into the "Last Version" file in
  // |user_data_dir|.
  void UpdateLastVersion(const base::FilePath& user_data_dir);

  // Schedules a search for the removal of any directories moved aside by
  // ProcessDowngrade. This operation is idempotent, and may be safely called
  // when no such directories exist.
  void DeleteMovedUserDataSoon(const base::FilePath& user_data_dir);

  // Process a previously-detected downgrade of |user_data_dir|. This must be
  // called late in shutdown while the process singleton is still held.
  void ProcessDowngrade(const base::FilePath& user_data_dir);

 private:
  enum class Type {
    kNone = 0,
    kAdministrativeWipe = 1,
    kUnsupported = 2,
    kMaxValue = kUnsupported
  };

  Type type_ = Type::kNone;
};

}  // namespace downgrade

#endif  // CHROME_BROWSER_DOWNGRADE_DOWNGRADE_MANAGER_H_
