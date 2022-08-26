// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "components/services/app_service/public/cpp/types_util.h"

namespace apps::deduplication {

AppDeduplicationService::AppDeduplicationService(Profile* profile)
    : profile_(profile) {
  app_provisioning_data_observeration_.Observe(
      AppProvisioningDataManager::Get());
  app_registry_cache_observation_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(profile)
           ->AppRegistryCache());
}

AppDeduplicationService::~AppDeduplicationService() = default;

std::vector<Entry> AppDeduplicationService::GetDuplicates(
    const EntryId& entry_id) {
  // TODO(b/238394602): Add logic to handle url entry id and web apps.
  std::vector<Entry> entries;

  auto it = entry_to_group_map_.find(entry_id);

  if (it == entry_to_group_map_.end()) {
    return entries;
  }
  uint32_t duplication_index = it->second;
  const auto& group = duplication_map_.find(duplication_index);
  if (group == duplication_map_.end()) {
    return entries;
  }

  for (const auto& entry : group->second.entries) {
    auto status_it = entry_status_.find(entry.entry_id);
    if (status_it == entry_status_.end()) {
      continue;
    }
    if (status_it->second == EntryStatus::kNonApp ||
        status_it->second == EntryStatus::kInstalledApp) {
      entries.push_back(entry);
    }
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
  uint32_t duplication_index_1 = it_1->second;

  auto it_2 = entry_to_group_map_.find(entry_id_2);
  if (it_2 == entry_to_group_map_.end()) {
    return false;
  }
  uint32_t duplication_index_2 = it_2->second;

  return duplication_index_1 == duplication_index_2;
}

void AppDeduplicationService::OnDuplicatedGroupListUpdated(
    const proto::DuplicatedGroupList& duplicated_group_list) {
  // Use the index as the internal indexing key for fast look up. If the
  // size of the duplicated groups goes over integer 32 limit, a new indexing
  // key needs to be introduced.
  uint32_t index = 1;
  for (auto const& group : duplicated_group_list.duplicate_group()) {
    DuplicateGroup duplicate_group;
    for (auto const& app : group.app()) {
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

      entry_to_group_map_[entry_id] = index;
      // Initialize entry status.
      entry_status_[entry_id] = entry_id.entry_type == EntryType::kApp
                                    ? EntryStatus::kNotInstalledApp
                                    : EntryStatus::kNonApp;

      Entry entry(std::move(entry_id));
      duplicate_group.entries.push_back(std::move(entry));
    }
    duplication_map_[index] = std::move(duplicate_group);
    index++;
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->AppRegistryCache().ForEachApp([this](const apps::AppUpdate& update) {
    UpdateInstallationStatus(update);
  });
}

void AppDeduplicationService::OnAppUpdate(const apps::AppUpdate& update) {
  UpdateInstallationStatus(update);
}

void AppDeduplicationService::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observation_.Reset();
}

void AppDeduplicationService::UpdateInstallationStatus(
    const apps::AppUpdate& update) {
  EntryId entry_id(update.PublisherId(), update.AppType());
  auto it = entry_status_.find(entry_id);

  if (it == entry_status_.end()) {
    return;
  }

  it->second = apps_util::IsInstalled(update.Readiness())
                   ? EntryStatus::kInstalledApp
                   : EntryStatus::kNotInstalledApp;
}

}  // namespace apps::deduplication
