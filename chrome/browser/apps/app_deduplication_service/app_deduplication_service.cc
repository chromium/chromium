// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/types_util.h"

namespace {
// Folder path to where the deduplication data will be stored on disk.
constexpr char kAppDeduplicationFolderPath[] =
    "app_deduplication_service/deduplication_data/";
}  // namespace

namespace apps::deduplication {

namespace prefs {
constexpr char kLastGetDataFromServerTimestamp[] =
    "apps.app_deduplication_service.last_get_data_from_server_timestamp";
}  // namespace prefs

AppDeduplicationService::AppDeduplicationService(Profile* profile)
    : profile_(profile),
      server_connector_(std::make_unique<AppDeduplicationServerConnector>()) {
  app_provisioning_data_observeration_.Observe(
      AppProvisioningDataManager::Get());
  app_registry_cache_observation_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(profile)
           ->AppRegistryCache());

  if (base::FeatureList::IsEnabled(features::kAppDeduplicationServiceFondue)) {
    base::FilePath path =
        profile_->GetPath().AppendASCII(kAppDeduplicationFolderPath);
    cache_ = std::make_unique<AppDeduplicationCache>(path);
    StartLoginFlow();
  }
}

AppDeduplicationService::~AppDeduplicationService() = default;

void AppDeduplicationService::StartLoginFlow() {
  const int hours_diff =
      std::abs((GetServerPref() - base::Time::Now()).InHours());

  if (hours_diff >= 24) {
    GetDeduplicateDataFromServer();
  } else {
    // Read most recent data from cache.
    cache_->ReadDeduplicationCache(base::BindOnce(
        &AppDeduplicationService::OnReadDeduplicationCacheCompleted,
        weak_ptr_factory_.GetWeakPtr()));
  }
}

void AppDeduplicationService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimePref(prefs::kLastGetDataFromServerTimestamp,
                             base::Time());
}

std::vector<Entry> AppDeduplicationService::GetDuplicates(
    const EntryId& entry_id) {
  std::vector<Entry> entries;

  absl::optional<uint32_t> duplication_index = FindDuplicationIndex(entry_id);
  if (!duplication_index.has_value()) {
    return entries;
  }
  const auto& group = duplication_map_.find(duplication_index.value());
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
  // TODO(b/238394602): Add interface with more than 2 entry ids.
  absl::optional<uint32_t> duplication_index_1 =
      FindDuplicationIndex(entry_id_1);
  if (!duplication_index_1.has_value()) {
    return false;
  }

  absl::optional<uint32_t> duplication_index_2 =
      FindDuplicationIndex(entry_id_2);
  if (!duplication_index_2.has_value()) {
    return false;
  }

  return duplication_index_1 == duplication_index_2;
}

// This function is only used when the kAppDeduplicationService flag
// is enabled.
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
      // TODO(b/238394602): Add server data verification.
      if (source == "arc") {
        entry_id = EntryId(app_id, AppType::kArc);
      } else if (source == "web") {
        entry_id = EntryId(app_id, AppType::kWeb);
      } else if (source == "phonehub") {
        entry_id = EntryId(app_id);
      } else if (source == "website") {
        GURL entry_url = GURL(app_id);
        if (entry_url.is_valid()) {
          entry_id = EntryId(GURL(app_id));
        } else {
          continue;
        }
      } else {
        continue;
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

absl::optional<uint32_t> AppDeduplicationService::FindDuplicationIndex(
    const EntryId& entry_id) {
  // TODO(b/238394602): Add logic to handle url entry id and web apps.
  // Check if there is an exact match of the entry id.
  auto it = entry_to_group_map_.find(entry_id);

  if (it != entry_to_group_map_.end()) {
    return it->second;
  }

  // For website, check if the url is in the scope of the recorded url in the
  // deduplication database. Here we assume all the websites has it's own entry.
  GURL entry_url = GURL(entry_id.id);
  if (entry_id.entry_type == EntryType::kWebPage && entry_url.is_valid()) {
    for (const auto& [recorded_entry_id, group_id] : entry_to_group_map_) {
      if (recorded_entry_id.entry_type != EntryType::kWebPage) {
        continue;
      }
      GURL recorded_entry_url = GURL(recorded_entry_id.id);
      if (!recorded_entry_url.is_valid()) {
        continue;
      }
      if (entry_url.scheme().empty() || recorded_entry_url.scheme().empty() ||
          entry_url.scheme() != recorded_entry_url.scheme()) {
        continue;
      }
      if (entry_url.host().empty() || recorded_entry_url.host().empty() ||
          entry_url.host() != recorded_entry_url.host()) {
        continue;
      }
      if (!entry_url.has_path() || !recorded_entry_url.has_path()) {
        continue;
      }
      if (base::StartsWith(entry_url.path(), recorded_entry_url.path(),
                           base::CompareCase::INSENSITIVE_ASCII)) {
        return group_id;
      }
    }
  }

  return absl::nullopt;
}

void AppDeduplicationService::GetDeduplicateDataFromServer() {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile_->GetURLLoaderFactory();
  if (!url_loader_factory.get()) {
    // `url_loader_factory` should only be null if we are in a non-dedupe
    // related test. Tests that use profile builder to create their profile
    // won't have `url_loader_factory` set up by default, so we bypass dedupes
    // code being called for those tests.
    CHECK_IS_TEST();
    return;
  }
  server_connector_->GetDeduplicateAppsFromServer(
      url_loader_factory,
      base::BindOnce(
          &AppDeduplicationService::OnGetDeduplicateDataFromServerCompleted,
          weak_ptr_factory_.GetWeakPtr()));
}

void AppDeduplicationService::OnGetDeduplicateDataFromServerCompleted(
    absl::optional<proto::DeduplicateData> response) {
  if (response.has_value()) {
    profile_->GetPrefs()->SetTime(prefs::kLastGetDataFromServerTimestamp,
                                  base::Time::Now());
    cache_->WriteDeduplicationCache(
        response.value(),
        base::BindOnce(
            &AppDeduplicationService::OnWriteDeduplicationCacheCompleted,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    cache_->ReadDeduplicationCache(base::BindOnce(
        &AppDeduplicationService::OnReadDeduplicationCacheCompleted,
        weak_ptr_factory_.GetWeakPtr()));
  }

  if (get_data_complete_callback_for_testing_) {
    std::move(get_data_complete_callback_for_testing_)
        .Run(response.has_value());
  }
}

void AppDeduplicationService::OnWriteDeduplicationCacheCompleted(bool result) {
  if (!result) {
    LOG(ERROR) << "Writing deduplication data to disk failed.";
    return;
  }
  cache_->ReadDeduplicationCache(base::BindOnce(
      &AppDeduplicationService::OnReadDeduplicationCacheCompleted,
      weak_ptr_factory_.GetWeakPtr()));
  return;
}

void AppDeduplicationService::OnReadDeduplicationCacheCompleted(
    absl::optional<proto::DeduplicateData> data) {
  if (!data.has_value()) {
    LOG(ERROR) << "Reading deduplication data from disk failed.";
    return;
  }
  DeduplicateDataToEntries(data.value());
}

base::Time AppDeduplicationService::GetServerPref() {
  return profile_->GetPrefs()->GetTime(prefs::kLastGetDataFromServerTimestamp);
}

// This function is only used when the kAppDeduplicationServiceFondue flag
// is enabled.
void AppDeduplicationService::DeduplicateDataToEntries(
    const proto::DeduplicateData data) {
  // Use the index as the internal indexing key for fast look up. If the
  // size of the duplicated groups goes over integer 32 limit, a new indexing
  // key needs to be introduced.
  uint32_t index = 1;
  for (auto const& group : data.app_group()) {
    DuplicateGroup duplicate_group;
    for (auto const& app : group.app()) {
      const std::string& app_id = app.app_id();
      const std::string& platform = app.platform();
      EntryId entry_id;

      if (platform == "arc") {
        entry_id = EntryId(app_id, AppType::kArc);
      } else if (platform == "web") {
        entry_id = EntryId(app_id, AppType::kWeb);
      } else if (platform == "phonehub") {
        entry_id = EntryId(app_id);
      } else if (platform == "website") {
        GURL entry_url = GURL(app_id);
        if (entry_url.is_valid()) {
          entry_id = EntryId(GURL(app_id));
        } else {
          continue;
        }
      } else {
        continue;
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

}  // namespace apps::deduplication
