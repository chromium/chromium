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

void AppDeduplicationService::OnDuplicatedAppsMapUpdated(
    const proto::DuplicatedAppsMap& duplicated_apps_map) {
  for (auto const& group : duplicated_apps_map.duplicated_apps_map()) {
    DuplicateGroup duplicate_group;
    for (auto const& app : group.second.apps()) {
      const std::string& app_id = app.app_id_for_platform();
      const std::string& source = app.source_name();
      AppType app_type = AppType::kUnknown;
      // TODO(b/238394602): Add more data type when real data is ready.
      if (source == "arc") {
        app_type = AppType::kArc;
      } else if (source == "web") {
        app_type = AppType::kWeb;
      }
      EntryId entry_id(app_id, app_type);
      entry_to_group_map_[entry_id] = group.first;

      Entry entry(std::move(entry_id));
      duplicate_group.entries.push_back(std::move(entry));
    }
    duplication_map_[group.first] = std::move(duplicate_group);
  }
}

}  // namespace apps::deduplication
