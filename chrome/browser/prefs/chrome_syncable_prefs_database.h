// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_CHROME_SYNCABLE_PREFS_DATABASE_H_
#define CHROME_BROWSER_PREFS_CHROME_SYNCABLE_PREFS_DATABASE_H_

#include "components/sync_preferences/syncable_prefs_database.h"

#include "components/sync_preferences/common_syncable_prefs_database.h"

class ChromeSyncablePrefsDatabase
    : public sync_preferences::SyncablePrefsDatabase {
 public:
  // Return true if `pref_name` is syncable.
  bool IsPreferenceSyncable(const std::string& pref_name) const override;

 private:
  // This defines the list of preferences that are syncable across all
  // platforms.
  sync_preferences::CommonSyncablePrefsDatabase common_syncable_prefs_database_;
};

#endif  // CHROME_BROWSER_PREFS_CHROME_SYNCABLE_PREFS_DATABASE_H_
