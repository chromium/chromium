// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/settings_sync_util.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/storage/sync_value_store_cache.h"
#include "components/sync/protocol/app_setting_specifics.pb.h"
#include "components/sync/protocol/extension_setting_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/storage_frontend.h"

namespace extensions {

namespace settings_sync_util {

namespace {

base::WeakPtr<syncer::SyncableService> GetSyncableServiceOnBackendSequence(
    base::WeakPtr<SyncValueStoreCache> sync_cache,
    syncer::ModelType type) {
  DCHECK(IsOnBackendSequence());
  if (!sync_cache)
    return nullptr;
  return sync_cache->GetSyncableService(type)->AsWeakPtr();
}

void PopulateExtensionSettingSpecifics(
    const std::string& extension_id,
    const std::string& key,
    const base::Value& value,
    sync_pb::ExtensionSettingSpecifics* specifics) {
  specifics->set_extension_id(extension_id);
  specifics->set_key(key);
  {
    std::string value_as_json;
    base::JSONWriter::Write(value, &value_as_json);
    specifics->set_value(value_as_json);
  }
}

void PopulateAppSettingSpecifics(
    const std::string& extension_id,
    const std::string& key,
    const base::Value& value,
    sync_pb::AppSettingSpecifics* specifics) {
  PopulateExtensionSettingSpecifics(
      extension_id, key, value, specifics->mutable_extension_setting());
}

}  // namespace

syncer::SyncData CreateData(
    const std::string& extension_id,
    const std::string& key,
    const base::Value& value,
    syncer::ModelType type) {
  sync_pb::EntitySpecifics specifics;
  switch (type) {
    case syncer::EXTENSION_SETTINGS:
      PopulateExtensionSettingSpecifics(
          extension_id,
          key,
          value,
          specifics.mutable_extension_setting());
      break;

    case syncer::APP_SETTINGS:
      PopulateAppSettingSpecifics(
          extension_id,
          key,
          value,
          specifics.mutable_app_setting());
      break;

    default:
      NOTREACHED();
  }

  return syncer::SyncData::CreateLocalData(
      extension_id + "/" + key, key, specifics);
}

syncer::SyncChange CreateAdd(
    const std::string& extension_id,
    const std::string& key,
    const base::Value& value,
    syncer::ModelType type) {
  return syncer::SyncChange(
      FROM_HERE,
      syncer::SyncChange::ACTION_ADD,
      CreateData(extension_id, key, value, type));
}

syncer::SyncChange CreateUpdate(
    const std::string& extension_id,
    const std::string& key,
    const base::Value& value,
    syncer::ModelType type) {
  return syncer::SyncChange(
      FROM_HERE,
      syncer::SyncChange::ACTION_UPDATE,
      CreateData(extension_id, key, value, type));
}

syncer::SyncChange CreateDelete(
    const std::string& extension_id,
    const std::string& key,
    syncer::ModelType type) {
  base::DictionaryValue no_value;
  return syncer::SyncChange(
      FROM_HERE,
      syncer::SyncChange::ACTION_DELETE,
      CreateData(extension_id, key, no_value, type));
}

base::OnceCallback<base::WeakPtr<syncer::SyncableService>()>
GetSyncableServiceProvider(content::BrowserContext* context,
                           syncer::ModelType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(context);
  DCHECK(type == syncer::APP_SETTINGS || type == syncer::EXTENSION_SETTINGS);
  StorageFrontend* frontend = StorageFrontend::Get(context);
  // StorageFrontend can be null in tests.
  if (!frontend) {
    return base::BindOnce(
        []() { return base::WeakPtr<syncer::SyncableService>(); });
  }
  SyncValueStoreCache* sync_cache = static_cast<SyncValueStoreCache*>(
      frontend->GetValueStoreCache(settings_namespace::SYNC));
  DCHECK(sync_cache);
  return base::BindOnce(&GetSyncableServiceOnBackendSequence,
                        sync_cache->AsWeakPtr(), type);
}

}  // namespace settings_sync_util

}  // namespace extensions
