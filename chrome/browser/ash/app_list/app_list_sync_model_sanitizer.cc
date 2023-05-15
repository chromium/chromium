// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_list_sync_model_sanitizer.h"

#include <vector>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "components/sync/model/string_ordinal.h"

namespace app_list {

namespace {

// Prefix added to IDs of page breaks created by `AppListSyncModelSanitizer`.
// The prexis ensures that sanitizer can easily identify page breaks created
// during previous sanitization passes (and which can freely be removed during
// sanitization later on), without substantially changing the page breaks app
// list item sync data so page breaks can be recognized on older chrome versions
// that do not have "implicit" page breaks.
constexpr char kImplicitPageBreakIdPrefix[] = "Implicit###";

}  // namespace

AppListSyncModelSanitizer::AppListSyncModelSanitizer(
    AppListSyncableService* syncable_service)
    : syncable_service_(syncable_service) {}

AppListSyncModelSanitizer::~AppListSyncModelSanitizer() = default;

void AppListSyncModelSanitizer::SanitizePageBreaks(
    const std::set<std::string>& top_level_items,
    bool reset_page_breaks) {
  const std::vector<AppListSyncableService::SyncItem*> sync_items =
      syncable_service_->GetSortedTopLevelSyncItems();

  std::vector<std::string> page_breaks_to_remove;
  std::vector<syncer::StringOrdinal> page_breaks_to_add;
  std::map<std::string, syncer::StringOrdinal> resolved_duplicate_positions;

  int current_page_size = 0;
  syncer::StringOrdinal last_valid_position;
  for (size_t i = 0; i < sync_items.size(); ++i) {
    const AppListSyncableService::SyncItem* item = sync_items[i];
    const std::string item_id = item->item_id;
    syncer::StringOrdinal item_ordinal =
        resolved_duplicate_positions.count(item_id)
            ? resolved_duplicate_positions[item_id]
            : item->item_ordinal;
    // `AppListSyncableService::GetSortedTopLevelSyncItems()` filters out items
    // with an invalid position.
    DCHECK(item_ordinal.IsValid());

    if (item->item_type == sync_pb::AppListSpecifics::TYPE_PAGE_BREAK) {
      const bool implicit_page_break =
          base::StartsWith(item_id, kImplicitPageBreakIdPrefix);
      if (reset_page_breaks && !implicit_page_break) {
        // When resetting page breaks, remove non-implicit page breaks.
        // If the page break is required at this position, a new implicit page
        // break will be created later on, with the difference that the new page
        // break will be removable by he sanitizer.
        page_breaks_to_remove.push_back(item_id);
        continue;
      }

      // If the page break is after a full page, leave it in place.
      if (!implicit_page_break ||
          current_page_size ==
              ash::SharedAppListConfig::instance().GetMaxNumOfItemsPerPage()) {
        current_page_size = 0;
        last_valid_position = item_ordinal;
        continue;
      }

      // If page is not yet full, and the page break was added to sanitize page
      // size (i.e. were not a result of an explicit user action in pre
      // productivity launcher), remove the page break.
      page_breaks_to_remove.push_back(item_id);
      continue;
    }

    if (item->item_type != sync_pb::AppListSpecifics::TYPE_FOLDER &&
        item->item_type != sync_pb::AppListSpecifics::TYPE_APP) {
      last_valid_position = item_ordinal;
      continue;
    }

    // Ignore app list items that are not installed locally - this has potential
    // to cause page overflow within sync data on certain devices,but it better
    // matches legacy behavior where page structure was primarily based on local
    // model, and results in better page structure on the devices with similar
    // set of installed apps and the productivity launcher feature disabled
    // (including the case where productivity launcher feature is toggled on the
    // same device).
    // Alternatively, this method could calculate page size purely from sync
    // data - this would ensure consistent mapping from an app to a page, but
    // could also unexpectedly create partially filled pages where they did not
    // previously exist (for example, if sync contains items that are not
    // installed on a portion of the user's devices).
    if (!base::Contains(top_level_items, item_id)) {
      last_valid_position = item_ordinal;
      continue;
    }

    // The item overflows the current page - add a page break just before it.
    if (current_page_size ==
        ash::SharedAppListConfig::instance().GetMaxNumOfItemsPerPage()) {
      // The page break will be inserted before the current item, so the current
      // page contains the current item.
      current_page_size = 1;

      DCHECK(last_valid_position.IsValid());
      // If page break should be added between items with the same ordinal,
      // deduplicate the item ordinals before calculating the new page break
      // position.
      if (last_valid_position.Equals(item_ordinal)) {
        ResolveDuplicatePositionsStartingAtIndex(
            sync_items, i, last_valid_position, &resolved_duplicate_positions);
        DCHECK(resolved_duplicate_positions.count(item_id));

        item_ordinal = resolved_duplicate_positions[item_id];
        page_breaks_to_add.push_back(
            last_valid_position.CreateBetween(item_ordinal));
      } else {
        page_breaks_to_add.push_back(
            last_valid_position.CreateBetween(item_ordinal));
      }
    } else {
      ++current_page_size;
    }

    last_valid_position = item_ordinal;
  }

  for (const auto& resolved_position : resolved_duplicate_positions) {
    syncable_service_->GetModelUpdater()->SetItemPosition(
        resolved_position.first, resolved_position.second);
  }

  for (const auto& position : page_breaks_to_add) {
    syncable_service_->AddPageBreakItem(
        std::string(kImplicitPageBreakIdPrefix) +
            base::Uuid::GenerateRandomV4().AsLowercaseString(),
        position);
  }

  for (const auto& id : page_breaks_to_remove)
    syncable_service_->DeleteSyncItem(id);
}

void AppListSyncModelSanitizer::ResolveDuplicatePositionsStartingAtIndex(
    const std::vector<AppListSyncableService::SyncItem*>& sync_items,
    size_t starting_index,
    const syncer::StringOrdinal& starting_ordinal,
    std::map<std::string, syncer::StringOrdinal>* resolved_positions) {
  DCHECK_LT(starting_index, sync_items.size());
  // Find the next position distinct from `starting_ordinal`, starting at
  // `starting_index`.
  syncer::StringOrdinal next_valid_position;
  // Default to the case a distinct position is not found.
  size_t first_distinct_index = sync_items.size();
  for (size_t i = starting_index; i < sync_items.size(); ++i) {
    AppListSyncableService::SyncItem* item = sync_items[i];
    if (item->item_ordinal.IsValid() &&
        !item->item_ordinal.Equals(starting_ordinal)) {
      next_valid_position = item->item_ordinal;
      first_distinct_index = i;
      break;
    }
  }

  // If all items from starting position to the end of sync items list have the
  // same ordinal, next valid position may not have been set.
  if (!next_valid_position.IsValid())
    next_valid_position = starting_ordinal.CreateAfter();

  // Generate new ordinals for all detected duplicate items.
  for (size_t current_index = first_distinct_index - 1;
       current_index >= starting_index; --current_index) {
    next_valid_position = starting_ordinal.CreateBetween(next_valid_position);
    AppListSyncableService::SyncItem* item = sync_items[current_index];
    resolved_positions->emplace(item->item_id, next_valid_position);
  }
}

}  // namespace app_list
