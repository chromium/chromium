// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_local_data_batch_uploader.h"

#include "base/functional/callback.h"
#include "base/notimplemented.h"

ThemeLocalDataBatchUploader::ThemeLocalDataBatchUploader(
    ThemeSyncableService* theme_syncable_service) {}

void ThemeLocalDataBatchUploader::GetLocalDataDescription(
    base::OnceCallback<void(syncer::LocalDataDescription)> callback) {
  NOTIMPLEMENTED();
}

void ThemeLocalDataBatchUploader::TriggerLocalDataMigration() {
  NOTIMPLEMENTED();
}

void ThemeLocalDataBatchUploader::TriggerLocalDataMigrationForItems(
    std::vector<syncer::LocalDataItemModel::DataId> items) {
  NOTIMPLEMENTED();
}
