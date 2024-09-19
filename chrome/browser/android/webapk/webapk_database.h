// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_DATABASE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_DATABASE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/android/webapk/webapk_registrar.h"
#include "components/sync/model/data_type_store.h"

namespace syncer {
class DataTypeStoreService;
class ModelError;
class MetadataBatch;
class MetadataChangeList;
}  // namespace syncer

namespace webapk {
struct RegistryUpdateData;

// Provides Read/Write access to a DataTypeStore DB.
class WebApkDatabase {
 public:
  using ReportErrorCallback =
      base::RepeatingCallback<void(const syncer::ModelError&)>;

  WebApkDatabase(syncer::DataTypeStoreService* data_type_store_service,
                 ReportErrorCallback error_callback);
  WebApkDatabase(const WebApkDatabase&) = delete;
  WebApkDatabase& operator=(const WebApkDatabase&) = delete;
  ~WebApkDatabase();

  using RegistryOpenedCallback = base::OnceCallback<void(
      Registry registry,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch)>;
  // Open existing or create new DB. Read all data and return it via callback.
  void OpenDatabase(RegistryOpenedCallback callback);

  using CompletionCallback = base::OnceCallback<void(bool success)>;
  // Writes the update data and metadata change list to DB, returns a bool
  // result.
  void Write(const RegistryUpdateData& update_data,
             std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
             CompletionCallback callback);

  void DeleteAllDataAndMetadata(
      syncer::DataTypeStore::CallbackWithResult callback);

  bool is_opened() const { return opened_; }

 private:
  void OnDatabaseOpened(RegistryOpenedCallback callback,
                        const std::optional<syncer::ModelError>& error,
                        std::unique_ptr<syncer::DataTypeStore> store);
  void OnAllDataAndMetadataRead(
      RegistryOpenedCallback callback,
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> data_records,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void OnDataWritten(CompletionCallback callback,
                     const std::optional<syncer::ModelError>& error);

  void RecordSyncedWebApkCountHistogram(int num_web_apks) const;

  std::unique_ptr<syncer::DataTypeStore> store_;
  const raw_ptr<syncer::DataTypeStoreService, DanglingUntriaged>
      data_type_store_service_;
  ReportErrorCallback error_callback_;

  // Database is opened if store is created and all data read.
  bool opened_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebApkDatabase> weak_ptr_factory_{this};
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_DATABASE_H_
