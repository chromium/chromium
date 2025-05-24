// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_list_util.h"

#include "ash/public/cpp/app_list/app_list_types.h"

namespace app_list {
std::unique_ptr<ash::AppListItemMetadata> GenerateItemMetadataFromSyncItem(
    const app_list::AppListSyncableService::SyncItem& sync_item) {
  DCHECK(sync_item.item_type != sync_pb::AppListSpecifics::TYPE_PAGE_BREAK);

  auto item_meta_data = std::make_unique<ash::AppListItemMetadata>();
  item_meta_data->id = sync_item.item_id;
  item_meta_data->position = sync_item.item_ordinal;
  item_meta_data->is_folder =
      (sync_item.item_type == sync_pb::AppListSpecifics::TYPE_FOLDER);
  item_meta_data->name = sync_item.item_name;
  item_meta_data->folder_id = sync_item.parent_id;
  item_meta_data->icon_color = sync_item.item_color;

  return item_meta_data;
}

}  // namespace app_list
