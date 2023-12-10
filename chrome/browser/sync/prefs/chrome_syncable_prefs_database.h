// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PREFS_CHROME_SYNCABLE_PREFS_DATABASE_H_
#define CHROME_BROWSER_SYNC_PREFS_CHROME_SYNCABLE_PREFS_DATABASE_H_

#include <map>

#include "base/strings/string_piece.h"
#include "components/sync_preferences/common_syncable_prefs_database.h"
#include "components/sync_preferences/syncable_prefs_database.h"

namespace browser_sync {

class ChromeSyncablePrefsDatabase
    : public sync_preferences::SyncablePrefsDatabase {
 public:
  // Returns the metadata associated to the pref or null if `pref_name` is not
  // syncable.
  absl::optional<sync_preferences::SyncablePrefMetadata>
  GetSyncablePrefMetadata(const std::string& pref_name) const override;

  std::map<base::StringPiece, sync_preferences::SyncablePrefMetadata>
  GetAllSyncablePrefsForTest() const;

 private:
  // This defines the list of preferences that are syncable across all
  // platforms.
  sync_preferences::CommonSyncablePrefsDatabase common_syncable_prefs_database_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_PREFS_CHROME_SYNCABLE_PREFS_DATABASE_H_
