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
#include "components/sync/model/model_type_store.h"

namespace syncer {
class ModelError;
class MetadataBatch;
class MetadataChangeList;
}  // namespace syncer

namespace webapk {
class AbstractWebApkDatabaseFactory;
struct RegistryUpdateData;

// Provides Read/Write access to a ModelTypeStore DB.
class WebApkDatabase {
 public:
  using ReportErrorCallback =
      base::RepeatingCallback<void(const syncer::ModelError&)>;

  WebApkDatabase(AbstractWebApkDatabaseFactory* database_factory,
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

  bool is_opened() const { return opened_; }

 private:
  void OnDatabaseOpened(RegistryOpenedCallback callback,
                        const absl::optional<syncer::ModelError>& error,
                        std::unique_ptr<syncer::ModelTypeStore> store);
  void OnAllDataRead(
      RegistryOpenedCallback callback,
      const absl::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records);
  void OnAllMetadataRead(
      std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records,
      RegistryOpenedCallback callback,
      const absl::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  std::unique_ptr<syncer::ModelTypeStore> store_;
  const raw_ptr<AbstractWebApkDatabaseFactory, DanglingUntriaged>
      database_factory_;
  ReportErrorCallback error_callback_;

  // Database is opened if store is created and all data read.
  bool opened_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebApkDatabase> weak_ptr_factory_{this};
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_DATABASE_H_
