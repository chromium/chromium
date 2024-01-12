// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_LIST_SYNC_MODEL_SANITIZER_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_LIST_SYNC_MODEL_SANITIZER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "components/sync/model/string_ordinal.h"

namespace app_list {

// Utility class for sanitizing app list model sync state in response to model
// changes that are result of local user actions.
// The main goal is to ensure that sync app list data remains in a sane state
// after local changes.
// For example, for kProductivityLauncher, whose UI doesn't use, nor handle page
// breaks, `AppListSyncModelSanitizer` is used to add page breaks at positions
// where they'd be if launcher had legacy max page size, so app list model
// changes synced to devices that do not have kProductivityLauncher enabled
// result in a sane app list page structure.
//
// WARNING: The sanitizer may cause changes to app list sync, so it should not
// be used to sanitize model state in response to remote changes (i.e. model
// changes through app list sync), to reduce risk of sync feedback loop.
class AppListSyncModelSanitizer {
 public:
  explicit AppListSyncModelSanitizer(AppListSyncableService* syncable_service);
  AppListSyncModelSanitizer(const AppListSyncModelSanitizer&) = delete;
  AppListSyncModelSanitizer& operator=(const AppListSyncModelSanitizer&) =
      delete;
  ~AppListSyncModelSanitizer();

  // Updates page breaks in the app list sync data to ensure items in the app
  // list model respect legacy max page size - launcher UI ignores page breaks,
  // and will not itself manage pagination state. This method
  // ensures that app list model change synced to other devices have sane
  // pagination structure.
  // `reset_page_breaks` indicates whether all existing page breaks can be
  // removed. If false, only page breaks previously created by a model
  // sanitization can be removed.
  void SanitizePageBreaks(const std::set<std::string>& top_level_items,
                          bool reset_page_breaks);

 private:
  // For items in sync_items that have identical position ordinals starting at
  // `starting_index` creates new ordinal values to preserve their order in
  // `sync_items`. The new item ordinals will be added to `resolved_positions`
  // as an item ID -> ordinal value pair. This method will not update the
  // associated sync items.
  // Expects `sync_items` to be sorted by the item string ordinals (in
  // increasing order).
  void ResolveDuplicatePositionsStartingAtIndex(
      const std::vector<AppListSyncableService::SyncItem*>& sync_items,
      size_t starting_index,
      const syncer::StringOrdinal& starting_ordinal,
      std::map<std::string, syncer::StringOrdinal>* resolved_positions);

  const raw_ptr<AppListSyncableService> syncable_service_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_LIST_SYNC_MODEL_SANITIZER_H_
