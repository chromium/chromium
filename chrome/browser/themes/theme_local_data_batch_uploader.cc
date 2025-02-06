// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_local_data_batch_uploader.h"

#include <variant>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/service/local_data_description.h"

namespace {
constexpr char kThemesLocalDataItemModelId[] = "current-theme";
}  // namespace

ThemeLocalDataBatchUploader::ThemeLocalDataBatchUploader(
    ThemeLocalDataBatchUploaderDelegate* delegate)
    : delegate_(delegate) {
  CHECK(delegate_);
}

void ThemeLocalDataBatchUploader::GetLocalDataDescription(
    base::OnceCallback<void(syncer::LocalDataDescription)> callback) {
  syncer::LocalDataDescription desc;
  desc.type = syncer::THEMES;
  // Avoid offering batch upload for local default theme.
  if (base::FeatureList::IsEnabled(syncer::kThemesBatchUpload) &&
      HasNonDefaultSavedLocalTheme()) {
    syncer::LocalDataItemModel item;
    item.id = kThemesLocalDataItemModelId;
    desc.local_data_models.push_back(std::move(item));
  }
  std::move(callback).Run(desc);
}

void ThemeLocalDataBatchUploader::TriggerLocalDataMigration() {
  CHECK(base::FeatureList::IsEnabled(syncer::kThemesBatchUpload));
  // Avoid migrating local default theme.
  if (HasNonDefaultSavedLocalTheme()) {
    delegate_->ApplySavedLocalThemeIfExistsAndClear();
  }
}

void ThemeLocalDataBatchUploader::TriggerLocalDataMigrationForItems(
    std::vector<syncer::LocalDataItemModel::DataId> items) {
  if (items.empty()) {
    return;
  }
  CHECK_EQ(items.size(), 1U);
  CHECK_EQ(std::get<std::string>(items[0]), kThemesLocalDataItemModelId);
  TriggerLocalDataMigration();
}

bool ThemeLocalDataBatchUploader::HasNonDefaultSavedLocalTheme() const {
  std::optional<sync_pb::ThemeSpecifics> specifics =
      delegate_->GetSavedLocalTheme();
  return specifics && ThemeSyncableService::HasNonDefaultTheme(*specifics);
}
