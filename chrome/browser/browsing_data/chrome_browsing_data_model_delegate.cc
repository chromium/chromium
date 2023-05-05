// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"

#include <memory>

#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
std::vector<ChromeBrowsingDataModelDelegate::DelegateEntry>
IsolatedWebAppBrowsingDataToDelegateEntries(
    base::flat_map<url::Origin, int64_t> isolated_web_app_browsing_data) {
  std::vector<ChromeBrowsingDataModelDelegate::DelegateEntry> entries;
  for (auto const& [origin, size] : isolated_web_app_browsing_data) {
    entries.emplace_back(
        origin,
        static_cast<BrowsingDataModel::StorageType>(
            ChromeBrowsingDataModelDelegate::StorageType::kIsolatedWebApp),
        size);
  }
  return entries;
}
#endif

}  // namespace

// static
std::unique_ptr<ChromeBrowsingDataModelDelegate>
ChromeBrowsingDataModelDelegate::CreateForProfile(Profile* profile) {
  return base::WrapUnique(new ChromeBrowsingDataModelDelegate(
      profile, profile->GetDefaultStoragePartition()));
}

// static
std::unique_ptr<ChromeBrowsingDataModelDelegate>
ChromeBrowsingDataModelDelegate::CreateForStoragePartition(
    Profile* profile,
    content::StoragePartition* storage_partition) {
  return base::WrapUnique(
      new ChromeBrowsingDataModelDelegate(profile, storage_partition));
}

// static
void ChromeBrowsingDataModelDelegate::BrowsingDataAccessed(
    content::RenderFrameHost* rfh,
    BrowsingDataModel::DataKey data_key,
    StorageType storage_type,
    bool blocked) {
  content_settings::PageSpecificContentSettings::BrowsingDataAccessed(
      rfh, data_key, static_cast<BrowsingDataModel::StorageType>(storage_type),
      blocked);
}

ChromeBrowsingDataModelDelegate::ChromeBrowsingDataModelDelegate(
    Profile* profile,
    content::StoragePartition* storage_partition)
    : profile_(profile), storage_partition_(storage_partition) {}

ChromeBrowsingDataModelDelegate::~ChromeBrowsingDataModelDelegate() = default;

void ChromeBrowsingDataModelDelegate::GetAllDataKeys(
    base::OnceCallback<void(std::vector<DelegateEntry>)> callback) {
#if !BUILDFLAG(IS_ANDROID)
  auto* web_app_provider = web_app::WebAppProvider::GetForWebApps(profile_);
  if (web_app_provider && storage_partition_->GetConfig().is_default()) {
    web_app_provider->scheduler().GetIsolatedWebAppBrowsingData(
        base::BindOnce(&IsolatedWebAppBrowsingDataToDelegateEntries)
            .Then(std::move(callback)));
  } else {
    std::move(callback).Run({});
  }
#else
  std::move(callback).Run({});
#endif

  // TODO(crbug.com/1271155): Implement data retrieval for remaining data types.
}

void ChromeBrowsingDataModelDelegate::RemoveDataKey(
    BrowsingDataModel::DataKey data_key,
    BrowsingDataModel::StorageTypeSet storage_types,
    base::OnceClosure callback) {
  if (storage_types.Has(
          static_cast<BrowsingDataModel::StorageType>(StorageType::kTopics))) {
    // Topics can be deleted but not queried from disk as the creating origins
    // are hashed before being saved.
    const url::Origin* origin = absl::get_if<url::Origin>(&data_key);
    auto* browsing_topics_service =
        browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(profile_);
    browsing_topics_service->ClearTopicsDataForOrigin(*origin);
  }

  // TODO(crbug.com/1271155): Utilize the callback in remaining data
  // typesdeletion methods that require a callback.
  std::move(callback).Run();
}

absl::optional<BrowsingDataModel::DataOwner>
ChromeBrowsingDataModelDelegate::GetDataOwner(
    BrowsingDataModel::DataKey data_key,
    BrowsingDataModel::StorageType storage_type) const {
  switch (static_cast<StorageType>(storage_type)) {
    case StorageType::kIsolatedWebApp:
      CHECK(absl::holds_alternative<url::Origin>(data_key))
          << "Unsupported IWA DataKey type: " << data_key.index();
      return absl::get<url::Origin>(data_key);

    case StorageType::kTopics:
      CHECK(absl::holds_alternative<url::Origin>(data_key))
          << "Unsupported Topics DataKey type: " << data_key.index();
      return absl::get<url::Origin>(data_key).host();

    default:
      return absl::nullopt;
  }
}
