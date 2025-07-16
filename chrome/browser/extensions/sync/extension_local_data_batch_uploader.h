// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYNC_EXTENSION_LOCAL_DATA_BATCH_UPLOADER_H_
#define CHROME_BROWSER_EXTENSIONS_SYNC_EXTENSION_LOCAL_DATA_BATCH_UPLOADER_H_

#include "base/memory/raw_ptr.h"
#include "components/sync/service/data_type_local_data_batch_uploader.h"

class Profile;

namespace extensions {

class ExtensionLocalDataBatchUploader
    : public syncer::DataTypeLocalDataBatchUploader {
 public:
  explicit ExtensionLocalDataBatchUploader(Profile* profile);

  ExtensionLocalDataBatchUploader(const ExtensionLocalDataBatchUploader&) =
      delete;
  ExtensionLocalDataBatchUploader& operator=(
      const ExtensionLocalDataBatchUploader&) = delete;

  ~ExtensionLocalDataBatchUploader() override;

  // syncer::DataTypeLocalDataBatchUploader implementation.
  void GetLocalDataDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)> callback) override;
  void TriggerLocalDataMigration() override;
  void TriggerLocalDataMigrationForItems(
      std::vector<syncer::LocalDataItemModel::DataId> items) override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYNC_EXTENSION_LOCAL_DATA_BATCH_UPLOADER_H_
