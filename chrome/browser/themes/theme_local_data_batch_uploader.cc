// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_local_data_batch_uploader.h"

#include <variant>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_service_utils.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/webui/cr_components/theme_color_picker/customize_chrome_colors.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/service/local_data_description.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
std::string BuildTitle(const sync_pb::ThemeSpecifics& specifics) {
  if (specifics.use_custom_theme()) {
    return specifics.custom_theme_name();
  }
  if (specifics.has_ntp_background() &&
      !specifics.ntp_background().attribution_line_1().empty()) {
    return specifics.ntp_background().attribution_line_1();
  }
  if (specifics.has_grayscale_theme_enabled()) {
    return l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_GREY_DEFAULT_LABEL);
  }
  if (specifics.has_user_color_theme()) {
    auto it = std::ranges::find_if(
        kDynamicCustomizeChromeColors,
        [&](const DynamicColorInfo& dynamic_color) {
          return dynamic_color.color == specifics.user_color_theme().color() &&
                 dynamic_color.variant ==
                     ProtoEnumToBrowserColorVariant(
                         specifics.user_color_theme().browser_color_variant());
        });
    if (it != kDynamicCustomizeChromeColors.end()) {
      return l10n_util::GetStringFUTF8(IDS_NTP_COLORS_BATCH_UPLOAD_DESCRIPTION,
                                       l10n_util::GetStringUTF16(it->label_id));
    }
  }
  return l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL);
}
}  // namespace

// static
const char ThemeLocalDataBatchUploader::kThemesLocalDataItemModelId[] =
    "current-theme";

ThemeLocalDataBatchUploader::ThemeLocalDataBatchUploader(
    ThemeLocalDataBatchUploaderDelegate* delegate)
    : delegate_(delegate) {
  CHECK(delegate_);
}

void ThemeLocalDataBatchUploader::GetLocalDataDescription(
    base::OnceCallback<void(syncer::LocalDataDescription)> callback) {
  syncer::LocalDataDescription desc;
  desc.type = syncer::THEMES;
  if (!base::FeatureList::IsEnabled(syncer::kThemesBatchUpload)) {
    std::move(callback).Run(desc);
    return;
  }
  // Avoid offering batch upload for local default theme.
  std::optional<sync_pb::ThemeSpecifics> specifics =
      GetNonDefaultSavedLocalTheme();
  base::UmaHistogramBoolean("Theme.BatchUpload.HasLocalTheme",
                            specifics.has_value());
  if (specifics.has_value()) {
    syncer::LocalDataItemModel item;
    item.id = kThemesLocalDataItemModelId;
    item.title = BuildTitle(*specifics);
    desc.local_data_models.push_back(std::move(item));
  }
  std::move(callback).Run(std::move(desc));
}

void ThemeLocalDataBatchUploader::TriggerLocalDataMigration() {
  CHECK(base::FeatureList::IsEnabled(syncer::kThemesBatchUpload));
  // Avoid migrating local default theme.
  if (GetNonDefaultSavedLocalTheme()) {
    delegate_->ApplySavedLocalThemeIfExistsAndClear();
    base::UmaHistogramBoolean("Theme.BatchUpload.LocalThemeMigrationTriggered",
                              true);
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

std::optional<sync_pb::ThemeSpecifics>
ThemeLocalDataBatchUploader::GetNonDefaultSavedLocalTheme() const {
  std::optional<sync_pb::ThemeSpecifics> specifics =
      delegate_->GetSavedLocalTheme();
  return (specifics && ThemeSyncableService::HasNonDefaultTheme(*specifics))
             ? specifics
             : std::nullopt;
}
