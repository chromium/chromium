// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"

#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/content_settings/browser/page_specific_content_settings.h"

// static
std::unique_ptr<ChromeBrowsingDataModelDelegate>
ChromeBrowsingDataModelDelegate::CreateForProfile(Profile* profile) {
  return std::make_unique<ChromeBrowsingDataModelDelegate>(profile);
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
    Profile* profile)
    : profile_(profile) {}

ChromeBrowsingDataModelDelegate::~ChromeBrowsingDataModelDelegate() = default;

void ChromeBrowsingDataModelDelegate::GetAllDataKeys(
    base::OnceCallback<void(std::vector<DelegateEntry>)> callback) {
  // TODO(crbug.com/1271155): Implement data retrieval for remaining data types.
  std::move(callback).Run({});
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
