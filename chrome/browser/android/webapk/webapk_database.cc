// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_database.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "chrome/browser/android/webapk/proto/webapk_database.pb.h"
#include "chrome/browser/android/webapk/webapk_database_factory.h"
#include "chrome/browser/android/webapk/webapk_registry_update.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"

namespace webapk {
WebApkDatabase::WebApkDatabase(AbstractWebApkDatabaseFactory* database_factory,
                               ReportErrorCallback error_callback)
    : database_factory_(database_factory),
      error_callback_(std::move(error_callback)) {}

WebApkDatabase::~WebApkDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebApkDatabase::OpenDatabase(RegistryOpenedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!store_);

  syncer::OnceModelTypeStoreFactory store_factory =
      database_factory_->GetStoreFactory();

  std::move(store_factory)
      .Run(syncer::WEB_APPS,
           base::BindOnce(&WebApkDatabase::OnDatabaseOpened,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebApkDatabase::Write(
    const RegistryUpdateData& update_data,
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    CompletionCallback callback) {
  // TODO(parsam): implement
}

void WebApkDatabase::OnDatabaseOpened(
    RegistryOpenedCallback callback,
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApks LevelDB opening error: " << error->ToString();
    return;
  }

  store_ = std::move(store);
  store_->ReadAllData(base::BindOnce(&WebApkDatabase::OnAllDataRead,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

void WebApkDatabase::OnAllDataRead(
    RegistryOpenedCallback callback,
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApks LevelDB data read error: " << error->ToString();
    return;
  }

  store_->ReadAllMetadata(base::BindOnce(
      &WebApkDatabase::OnAllMetadataRead, weak_ptr_factory_.GetWeakPtr(),
      std::move(data_records), std::move(callback)));
}

void WebApkDatabase::OnAllMetadataRead(
    std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records,
    RegistryOpenedCallback callback,
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    error_callback_.Run(*error);
    DLOG(ERROR) << "WebApks LevelDB metadata read error: " << error->ToString();
    return;
  }

  Registry registry;
  for (const syncer::ModelTypeStore::Record& record : *data_records) {
    std::unique_ptr<WebApkProto> proto = std::make_unique<WebApkProto>();
    const bool parsed = proto->ParseFromString(record.value);
    if (!parsed) {
      DLOG(ERROR) << "WebApks LevelDB parse error: can't parse proto.";
    }

    registry.emplace(record.id, std::move(proto));
  }

  opened_ = true;
  // This should be a tail call: a callback code may indirectly call |this|
  // methods, like WebApkDatabase::Write()
  std::move(callback).Run(std::move(registry), std::move(metadata_batch));
}

}  // namespace webapk
