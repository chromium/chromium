// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service.h"

namespace apps::deduplication {

AppDeduplicationService::AppDeduplicationService(Profile* profile) {
  app_provisioning_data_observeration_.Observe(
      AppProvisioningDataManager::Get());
}

AppDeduplicationService::~AppDeduplicationService() = default;

std::vector<Entry> AppDeduplicationService::GetDuplicates(
    const EntryId& entry_id) {
  // TODO(b/238394602): Only return installed apps (might need to add a flag in
  // the interface to indicate).
  // TODO(b/238394602): Add logic to handle url entry id and web apps.
  std::vector<Entry> entries;

  auto it = entry_to_group_map_.find(entry_id);

  if (it == entry_to_group_map_.end()) {
    return entries;
  }
  std::string duplication_key = it->second;
  auto group = duplication_map_.find(duplication_key);
  if (group == duplication_map_.end()) {
    return entries;
  }

  for (const auto& entry : group->second.entries) {
    entries.push_back(entry);
  }
  return entries;
}

bool AppDeduplicationService::AreDuplicates(const EntryId& entry_id_1,
                                            const EntryId& entry_id_2) {
  // TODO(b/238394602): Add logic to handle url entry id and web apps.
  // TODO(b/238394602): Add interface with more than 2 entry ids.
  auto it_1 = entry_to_group_map_.find(entry_id_1);
  if (it_1 == entry_to_group_map_.end()) {
    return false;
  }
  const std::string& duplication_key_1 = it_1->second;

  auto it_2 = entry_to_group_map_.find(entry_id_2);
  if (it_2 == entry_to_group_map_.end()) {
    return false;
  }
  const std::string& duplication_key_2 = it_2->second;

  return duplication_key_1 == duplication_key_2;
}

void AppDeduplicationService::OnDuplicatedAppsMapUpdated(
    const proto::DuplicatedAppsMap& duplicated_apps_map) {
  for (auto const& group : duplicated_apps_map.duplicated_apps_map()) {
    DuplicateGroup duplicate_group;
    for (auto const& app : group.second.apps()) {
      const std::string& app_id = app.app_id_for_platform();
      const std::string& source = app.source_name();
      EntryId entry_id;
      // TODO(b/238394602): Add more data type when real data is ready.
      if (source == "arc") {
        entry_id = EntryId(app_id, AppType::kArc);
      } else if (source == "web") {
        entry_id = EntryId(app_id, AppType::kWeb);
      } else if (source == "phonehub") {
        entry_id = EntryId(app_id);
      }

      entry_to_group_map_[entry_id] = group.first;

      Entry entry(std::move(entry_id));
      duplicate_group.entries.push_back(std::move(entry));
    }
    duplication_map_[group.first] = std::move(duplicate_group);
  }
}

}  // namespace apps::deduplication
