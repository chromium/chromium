// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNGRADE_SNAPSHOT_FILE_COLLECTOR_H_
#define CHROME_BROWSER_DOWNGRADE_SNAPSHOT_FILE_COLLECTOR_H_

#include <stdint.h>

#include <vector>

#include "base/files/file_path.h"

namespace downgrade {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SnapshotItemId {
  kLocalState = 0,
  kHighResAvatar = 1,
  kLastBrowser = 2,
  kPreferences = 3,
  kSecurePreferences = 4,
  kHistory = 5,
  kFavicons = 6,
  kTopSites = 7,
  kLocalOrSyncableBookmarks = 8,
  kLegacyCurrentTabSession = 9,  // TODO(crbug.com/40704630): Remove in M89
  kLegacyCurrentSession = 10,    // TODO(crbug.com/40704630): Remove in M89
  kGAIAPicture = 11,
  kAffiliationDatabase = 12,
  kLoginDataForProfile = 13,
  kLoginDataForAccount = 14,
  kWebData = 15,
  kStrikeDatabase = 16,
  kCookie = 17,
  kProfileIcon = 18,
  kLastVersion = 19,
  kSessions = 20,
  kAccountBookmarks = 21,
  kMaxValue = kAccountBookmarks
};

struct SnapshotItemDetails {
  enum class ItemType { kFile, kDirectory };

  SnapshotItemDetails(base::FilePath path,
                      ItemType type,
                      uint64_t data_types,
                      SnapshotItemId id);
  ~SnapshotItemDetails() = default;
  const base::FilePath path;
  const bool is_directory;

  // Bitfield from ChromeBrowsingDataRemoverDelegate::DataType representing
  // the data types affected by this item.
  const uint64_t data_types;
  const SnapshotItemId id;
};

// Returns a list of items to snapshot that should be directly under the user
// data  directory.
std::vector<SnapshotItemDetails> CollectUserDataItems();

// Returns a list of items to snapshot that should be under a profile directory.
std::vector<SnapshotItemDetails> CollectProfileItems();

}  // namespace downgrade

#endif  // CHROME_BROWSER_DOWNGRADE_SNAPSHOT_FILE_COLLECTOR_H_
