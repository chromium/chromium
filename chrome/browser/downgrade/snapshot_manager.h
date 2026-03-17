// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNGRADE_SNAPSHOT_MANAGER_H_
#define CHROME_BROWSER_DOWNGRADE_SNAPSHOT_MANAGER_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/version.h"

namespace downgrade {

struct SnapshotItemDetails {
  enum class ItemType { kFile, kDirectory };

  SnapshotItemDetails(base::FilePath path, ItemType type, uint64_t data_types);
  ~SnapshotItemDetails() = default;
  const base::FilePath path;
  const bool is_directory;

  // Bitfield from ChromeBrowsingDataRemoverDelegate::DataType representing
  // the data types affected by this item.
  const uint64_t data_types;
};

class DowngradeManagerDelegate;

// Class that handles saving snapshots of some user data after a browser
// upgrade, and loading the appropriate snapshot after a downgrade.
class SnapshotManager {
 public:
  // Instantiates a SnapshotManager that will handle taking a snapshot and
  // restoring snapshots for the data from |user_data_dir| for the current
  // chrome version.
  SnapshotManager(const base::FilePath& user_data_dir,
                  const DowngradeManagerDelegate* delegate);

  SnapshotManager(const SnapshotManager& other) = delete;
  SnapshotManager& operator=(const SnapshotManager&) = delete;

  virtual ~SnapshotManager();

  // Copies specified files from |user_data_dir_| for |version| into the
  // appropriate snapshot directory.
  void TakeSnapshot(const base::Version& version);

  // Restores all the files from the highest available snapshot version that is
  // not greater than |version| to |user_data_dir_|. If the highest available
  // snapshot version is equal to |version|, the snapshot is moved, otherwise
  // it is copied. If the snapshot folder is empty after this operation, it is
  // moved for later deletion.
  void RestoreSnapshot(const base::Version& version);

  // Keeps the number of snapshots on the disk under |max_number_of_snapshots|
  // by moving invalid and older snapshots for later deletion. If |milestone| is
  // specified, limit the deletion to the snapshots from that milestone.
  void PurgeInvalidAndOldSnapshots(int max_number_of_snapshots,
                                   std::optional<uint32_t> milestone) const;

  // Deletes snapshot data created after |delete_begin| for |profile_base_name|.
  // If |files_to_delete| is nullopt, all data for the profile is removed.
  void DeleteSnapshotDataForProfile(
      base::Time delete_begin,
      const base::FilePath& profile_base_name,
      std::optional<std::vector<base::FilePath>> files_to_delete);

 private:
  const base::FilePath user_data_dir_;
  const raw_ptr<const DowngradeManagerDelegate> delegate_;
};

}  // namespace downgrade

#endif  // CHROME_BROWSER_DOWNGRADE_SNAPSHOT_MANAGER_H_
