// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_DATABASE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_DATABASE_H_

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
      base::RepeatingCallback<void(const syncer::ModelError)>;

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
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_DATABASE_H_
