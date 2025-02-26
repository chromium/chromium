// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_LOCAL_DATA_BATCH_UPLOADER_H_
#define CHROME_BROWSER_THEMES_THEME_LOCAL_DATA_BATCH_UPLOADER_H_

#include "base/functional/callback_forward.h"
#include "components/sync/service/data_type_local_data_batch_uploader.h"
#include "components/sync/service/local_data_description.h"

namespace sync_pb {
class ThemeSpecifics;
}  // namespace sync_pb

class ThemeLocalDataBatchUploaderDelegate {
 public:
  // Returns the saved local theme if it exists. This refers to the local theme
  // that existed prior to receiving the remote theme from sync. The local theme
  // is reapplied if sync stops. It is received as a ThemeSpecifics since that
  // best encapsulates the different types of themes.
  virtual std::optional<sync_pb::ThemeSpecifics> GetSavedLocalTheme() const = 0;

  // Applies the saved local theme if it exists. This returns true iff a saved
  // local theme existed.
  virtual bool ApplySavedLocalThemeIfExistsAndClear() = 0;
};

class ThemeLocalDataBatchUploader
    : public syncer::DataTypeLocalDataBatchUploader {
 public:
  // ID for the themes batch upload data item. There can only ever be at most
  // one item.
  static const char kThemesLocalDataItemModelId[];

  explicit ThemeLocalDataBatchUploader(
      ThemeLocalDataBatchUploaderDelegate* delegate);
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

 private:
  std::optional<sync_pb::ThemeSpecifics> GetNonDefaultSavedLocalTheme() const;

  raw_ptr<ThemeLocalDataBatchUploaderDelegate> delegate_;
};

#endif  // CHROME_BROWSER_THEMES_THEME_LOCAL_DATA_BATCH_UPLOADER_H_
