// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_REORDER_APP_LIST_REORDER_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_REORDER_APP_LIST_REORDER_UTIL_H_

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "components/sync/model/string_ordinal.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace app_list {
namespace reorder {

// If the entropy (i.e. the ratio of the number of items out of order to the
// total number) is greater than this value, the sort order is reset to kCustom.
extern const float kOrderResetThreshold;

struct ReorderParam {
  ReorderParam(const std::string& new_sync_item_id,
               const syncer::StringOrdinal& new_ordinal);
  ReorderParam(const ReorderParam&);
  ~ReorderParam();

  // The sync item id.
  std::string sync_item_id;

  // The new ordinal for the sync item identified by `sync_item_id`.
  syncer::StringOrdinal ordinal;
};

// Wrapping a sync item for comparison.
template <typename T>
struct SyncItemWrapper {
  explicit SyncItemWrapper(const AppListSyncableService::SyncItem& sync_item);
  explicit SyncItemWrapper(const ash::AppListItemMetadata& metadata);

  std::string id;
  syncer::StringOrdinal item_ordinal;
  bool is_folder = false;

  // The attribute for comparison.
  T key_attribute;
};

// A comparator class used to compare ash::IconColor wrapper.
class IconColorWrapperComparator {
 public:
  IconColorWrapperComparator();

  // Returns true if lhs precedes rhs.
  bool operator()(const reorder::SyncItemWrapper<ash::IconColor>& lhs,
                  const reorder::SyncItemWrapper<ash::IconColor>& rhs) const;
};

// A comparator class used to compare std::u16string wrapper.
class StringWrapperComparator {
 public:
  StringWrapperComparator(bool increasing, icu::Collator* collator);

  // Returns true if lhs precedes rhs.
  bool operator()(const reorder::SyncItemWrapper<std::u16string>& lhs,
                  const reorder::SyncItemWrapper<std::u16string>& rhs) const;

 private:
  const bool increasing_;
  const raw_ptr<icu::Collator> collator_;
};

struct EphemeralAwareName {
  EphemeralAwareName(bool is_ephemeral, std::u16string name);
  ~EphemeralAwareName();

  bool is_ephemeral;
  std::u16string name;
};

// A comparator class used to compare EphemeralAwareName wrapper.
class EphemeralStateAndNameComparator {
 public:
  explicit EphemeralStateAndNameComparator(icu::Collator* collator);

  // Returns true if lhs precedes rhs.
  bool operator()(
      const reorder::SyncItemWrapper<EphemeralAwareName>& lhs,
      const reorder::SyncItemWrapper<EphemeralAwareName>& rhs) const;

 private:
  const raw_ptr<icu::Collator> collator_;
};

// Gets a list of wrappers based on the mappings from ids to sync items.
template <typename T>
std::vector<SyncItemWrapper<T>> GenerateWrappersFromSyncItems(
    const AppListSyncableService::SyncItemMap& sync_item_map) {
  std::vector<SyncItemWrapper<T>> wrappers;
  for (const auto& id_item_pair : sync_item_map) {
    auto* sync_item = id_item_pair.second.get();

    if (sync_item->item_type == sync_pb::AppListSpecifics::TYPE_PAGE_BREAK)
      continue;

    wrappers.emplace_back(*sync_item);
  }

  return wrappers;
}

// Gets a list of sync item wrappers based on the given app list items. The item
// with the ignored id should not be included in the return list.
template <typename T>
std::vector<SyncItemWrapper<T>> GenerateWrappersFromAppListItems(
    const std::vector<const ChromeAppListItem*>& app_list_items,
    const std::optional<std::string>& ignored_id) {
  std::vector<SyncItemWrapper<T>> wrappers;
  for (const auto* app_list_item : app_list_items) {
    if (ignored_id && *ignored_id == app_list_item->id())
      continue;

    wrappers.emplace_back(app_list_item->metadata());
  }
  return wrappers;
}

// SyncItemWrapper<std::u16string> ---------------------------------------------

template <>
SyncItemWrapper<std::u16string>::SyncItemWrapper(
    const AppListSyncableService::SyncItem& sync_item);
template <>
SyncItemWrapper<std::u16string>::SyncItemWrapper(
    const ash::AppListItemMetadata& metadata);

// SyncItemWrapper<ash::IconColor> ---------------------------------------------

template <>
SyncItemWrapper<ash::IconColor>::SyncItemWrapper(
    const AppListSyncableService::SyncItem& sync_item);
template <>
SyncItemWrapper<ash::IconColor>::SyncItemWrapper(
    const ash::AppListItemMetadata& metadata);

// SyncItemWrapper<EphemeralAwareName> -----------------------------------------

template <>
SyncItemWrapper<EphemeralAwareName>::SyncItemWrapper(
    const AppListSyncableService::SyncItem& sync_item);
template <>
SyncItemWrapper<EphemeralAwareName>::SyncItemWrapper(
    const ash::AppListItemMetadata& metadata);

}  // namespace reorder
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_REORDER_APP_LIST_REORDER_UTIL_H_
