// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_non_recording_data_store.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_noop_data_writer.h"

namespace resource_coordinator {

LocalSiteCharacteristicsNonRecordingDataStore::
    LocalSiteCharacteristicsNonRecordingDataStore(
        Profile* profile,
        LocalSiteCharacteristicsDataStoreInspector* data_store_inspector,
        SiteCharacteristicsDataStore* data_store_for_readers)
    : data_store_for_readers_(data_store_for_readers),
      data_store_inspector_(data_store_inspector),
      profile_(profile) {
  DCHECK(data_store_for_readers_);
  // Register the debug interface against the profile.
  LocalSiteCharacteristicsDataStoreInspector::SetForProfile(this, profile);
}

LocalSiteCharacteristicsNonRecordingDataStore::
    ~LocalSiteCharacteristicsNonRecordingDataStore() {
  LocalSiteCharacteristicsDataStoreInspector::SetForProfile(nullptr, profile_);
}

std::unique_ptr<SiteCharacteristicsDataReader>
LocalSiteCharacteristicsNonRecordingDataStore::GetReaderForOrigin(
    const url::Origin& origin) {
  return data_store_for_readers_->GetReaderForOrigin(origin);
}

std::unique_ptr<SiteCharacteristicsDataWriter>
LocalSiteCharacteristicsNonRecordingDataStore::GetWriterForOrigin(
    const url::Origin& origin,
    performance_manager::TabVisibility tab_visibility) {
  // Return a fake data writer.
  SiteCharacteristicsDataWriter* writer =
      new LocalSiteCharacteristicsNoopDataWriter();
  return base::WrapUnique(writer);
}

bool LocalSiteCharacteristicsNonRecordingDataStore::IsRecordingForTesting() {
  return false;
}

const char* LocalSiteCharacteristicsNonRecordingDataStore::GetDataStoreName() {
  return "LocalSiteCharacteristicsNonRecordingDataStore";
}

std::vector<url::Origin>
LocalSiteCharacteristicsNonRecordingDataStore::GetAllInMemoryOrigins() {
  if (!data_store_inspector_)
    return std::vector<url::Origin>();

  return data_store_inspector_->GetAllInMemoryOrigins();
}

void LocalSiteCharacteristicsNonRecordingDataStore::GetDatabaseSize(
    DatabaseSizeCallback on_have_data) {
  if (!data_store_inspector_) {
    std::move(on_have_data).Run(base::nullopt, base::nullopt);
    return;
  }

  data_store_inspector_->GetDatabaseSize(std::move(on_have_data));
}

bool LocalSiteCharacteristicsNonRecordingDataStore::GetDataForOrigin(
    const url::Origin& origin,
    bool* is_dirty,
    std::unique_ptr<SiteDataProto>* data) {
  if (!data_store_inspector_)
    return false;

  return data_store_inspector_->GetDataForOrigin(origin, is_dirty, data);
}

SiteCharacteristicsDataStore*
LocalSiteCharacteristicsNonRecordingDataStore::GetDataStore() {
  return this;
}

}  // namespace resource_coordinator
