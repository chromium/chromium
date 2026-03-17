// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNGRADE_DOWNGRADE_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_DOWNGRADE_DOWNGRADE_MANAGER_DELEGATE_H_

#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/downgrade/snapshot_manager.h"

namespace downgrade {

class DowngradeManagerDelegate {
 public:
  virtual ~DowngradeManagerDelegate() = default;

  // Returns the maximum number of snapshots to retain.
  virtual int GetMaxNumberOfSnapshots() const = 0;

  // Returns true if user data snapshotting is enabled.
  virtual bool UserDataSnapshotEnabled() const = 0;

  // Returns the disk cache directory override, or an empty path.
  virtual base::FilePath GetDiskCacheDir() const = 0;

  // Returns the list of items to snapshot that are under the user data
  // directory.
  virtual std::vector<SnapshotItemDetails> GetUserDataSnapshotItems() const = 0;

  // Returns the list of items to snapshot that are under a profile directory.
  virtual std::vector<SnapshotItemDetails> GetProfileSnapshotItems() const = 0;
};

}  // namespace downgrade

#endif  // CHROME_BROWSER_DOWNGRADE_DOWNGRADE_MANAGER_DELEGATE_H_
