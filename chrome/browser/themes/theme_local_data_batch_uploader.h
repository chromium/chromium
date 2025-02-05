// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_LOCAL_DATA_BATCH_UPLOADER_H_
#define CHROME_BROWSER_THEMES_THEME_LOCAL_DATA_BATCH_UPLOADER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "components/sync/service/data_type_local_data_batch_uploader.h"
#include "components/sync/service/local_data_description.h"

class ThemeLocalDataBatchUploader
    : public syncer::DataTypeLocalDataBatchUploader {
 public:
  explicit ThemeLocalDataBatchUploader(
      ThemeSyncableService* theme_syncable_service);

  // Retrieves information about the existing local data.
  void GetLocalDataDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)> callback) override;

  // Triggers the process of moving the data. The process is in fact async,
  // but no notion of completion is exposed here.
  void TriggerLocalDataMigration() override;

  // Triggers the process of moving the data restricted to the data that matches
  // the `syncer::LocalDataItemModel::DataId` in `items`. The process is in fact
  // async, but no notion of completion is exposed here.
  void TriggerLocalDataMigrationForItems(
      std::vector<syncer::LocalDataItemModel::DataId> items) override;
};

#endif  // CHROME_BROWSER_THEMES_THEME_LOCAL_DATA_BATCH_UPLOADER_H_
