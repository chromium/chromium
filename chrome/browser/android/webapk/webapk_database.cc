// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_database.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "chrome/browser/android/webapk/proto/webapk_database.pb.h"
#include "chrome/browser/android/webapk/webapk_registry_update.h"
#include "chrome/browser/android/webapk/webapk_sync_bridge.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"

namespace webapk {
WebApkDatabase::WebApkDatabase(
    syncer::DataTypeStoreService* data_type_store_service,
    ReportErrorCallback error_callback)
    : data_type_store_service_(data_type_store_service),
      error_callback_(std::move(error_callback)) {
  CHECK(data_type_store_service_);
}

WebApkDatabase::~WebApkDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebApkDatabase::OpenDatabase(RegistryOpenedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!store_);

  syncer::OnceDataTypeStoreFactory store_factory =
      data_type_store_service_->GetStoreFactory();

  std::move(store_factory)
      .Run(syncer::WEB_APPS,
           base::BindOnce(&WebApkDatabase::OnDatabaseOpened,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebApkDatabase::Write(
    const RegistryUpdateData& update_data,
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    CompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(opened_);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  // |update_data| can be empty here but we should write |metadata_change_list|
  // anyway.
  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));

  for (const std::unique_ptr<WebApkProto>& webapk :
       update_data.apps_to_create) {
    write_batch->WriteData(
        ManifestIdStrToAppId(webapk->sync_data().manifest_id()),
        webapk->SerializeAsString());
  }

  for (const std::string& app_id : update_data.apps_to_delete) {
    write_batch->DeleteData(app_id);
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&WebApkDatabase::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebApkDatabase::DeleteAllDataAndMetadata(
    syncer::DataTypeStore::CallbackWithResult callback) {
  CHECK(store_);
  store_->DeleteAllDataAndMetadata(std::move(callback));
}

void WebApkDatabase::OnDatabaseOpened(
    RegistryOpenedCallback callback,
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApks LevelDB opening error: " << error->ToString();
    return;
  }

  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(
      base::BindOnce(&WebApkDatabase::OnAllDataAndMetadataRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebApkDatabase::OnAllDataAndMetadataRead(
    RegistryOpenedCallback callback,
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> data_records,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApks LevelDB read error: " << error->ToString();
    return;
  }

  Registry registry;
  for (const syncer::DataTypeStore::Record& record : *data_records) {
    std::unique_ptr<WebApkProto> proto = std::make_unique<WebApkProto>();
    const bool parsed = proto->ParseFromString(record.value);
    if (!parsed) {
      DLOG(ERROR) << "WebApks LevelDB parse error: can't parse proto.";
    }

    registry.emplace(record.id, std::move(proto));
  }

  RecordSyncedWebApkCountHistogram(registry.size());

  opened_ = true;
  // This should be a tail call: a callback code may indirectly call |this|
  // methods, like WebApkDatabase::Write()
  std::move(callback).Run(std::move(registry), std::move(metadata_batch));
}

void WebApkDatabase::OnDataWritten(
    CompletionCallback callback,
    const std::optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApks LevelDB write error: " << error->ToString();
  }

  std::move(callback).Run(!error);
}

void WebApkDatabase::RecordSyncedWebApkCountHistogram(int num_web_apks) const {
  base::UmaHistogramExactLinear("WebApk.Sync.SyncedWebApkCount", num_web_apks,
                                51 /* max_count */);
}

}  // namespace webapk
