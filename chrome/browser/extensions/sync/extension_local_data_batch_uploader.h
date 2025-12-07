// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYNC_EXTENSION_LOCAL_DATA_BATCH_UPLOADER_H_
#define CHROME_BROWSER_EXTENSIONS_SYNC_EXTENSION_LOCAL_DATA_BATCH_UPLOADER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "components/sync/service/data_type_local_data_batch_uploader.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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
  // Uploads all locat extensions, or only those specified in `items` to the
  // current primary user's account.
  void TriggerLocalDataMigrationForItemsInternal(
      std::optional<ExtensionIdSet> ids_to_upload);

  const raw_ptr<Profile> profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYNC_EXTENSION_LOCAL_DATA_BATCH_UPLOADER_H_
