// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/drive_recent_file_suggestion_provider.h"

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "chromeos/ash/components/drivefs/drivefs_util.h"
#include "components/drive/file_errors.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/base/l10n/time_format.h"

namespace ash {
namespace {

constexpr base::TimeDelta kMaxLastModifiedTime = base::Days(8);

}  // namespace

DriveRecentFileSuggestionProvider::DriveRecentFileSuggestionProvider(
    Profile* profile,
    base::RepeatingCallback<void(FileSuggestionType)> notify_update_callback)
    : FileSuggestionProvider(notify_update_callback), profile_(profile) {}

DriveRecentFileSuggestionProvider::~DriveRecentFileSuggestionProvider() =
    default;

void DriveRecentFileSuggestionProvider::GetSuggestFileData(
    GetSuggestFileDataCallback callback) {
  const bool has_active_request =
      !on_drive_results_ready_callback_list_.empty();

  // Add `callback` to the waiting list.
  on_drive_results_ready_callback_list_.AddUnsafe(std::move(callback));

  // Return early if there is an active search request. `callback` will run when
  // the active search completes.
  if (has_active_request) {
    return;
  }

  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);

  // If there is not any available drive service, return early.
  if (!drive_service || !drive_service->IsMounted()) {
    on_drive_results_ready_callback_list_.Notify(
        /*suggest_results=*/std::nullopt);
    return;
  }

  auto query = drivefs::mojom::QueryParameters::New();
  query->sort_field = drivefs::mojom::QueryParameters::SortField::kLastModified;
  query->sort_direction =
      drivefs::mojom::QueryParameters::SortDirection::kDescending;
  query->query_source =
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
  query->modified_time = base::Time::Now() - kMaxLastModifiedTime;
  query->modified_time_operator =
      drivefs::mojom::QueryParameters::DateComparisonOperator::kGreaterThan;
  query->page_size = 10;

  drive_service->GetDriveFsHost()->PerformSearch(
      std::move(query),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&DriveRecentFileSuggestionProvider::OnSearchDriveFs,
                         weak_factory_.GetWeakPtr()),
          drive::FileError::FILE_ERROR_ABORT, absl::nullopt));
}

void DriveRecentFileSuggestionProvider::MaybeUpdateItemSuggestCache(
    base::PassKey<FileSuggestKeyedService>) {}

void DriveRecentFileSuggestionProvider::OnSearchDriveFs(
    drive::FileError error,
    absl::optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  if (error != drive::FileError::FILE_ERROR_OK || !items) {
    on_drive_results_ready_callback_list_.Notify(absl::nullopt);
    return;
  }

  std::vector<FileSuggestData> results;
  for (const auto& item : *items) {
    base::Time modified_time = item->metadata->modification_time;
    std::u16string relative_date =
        ui::TimeFormat::RelativeDate(modified_time, nullptr);
    // TODO(b/316180378): Localize justification strings.
    std::u16string justification;
    if (!relative_date.empty()) {
      justification = base::JoinString(
          {u"[Needs i18n] Modified", base::ToLowerASCII(relative_date)}, u" ");
    } else {
      justification = base::JoinString(
          {u"[Needs i18n] Modified", base::TimeFormatShortDate(modified_time)},
          u" ");
    }

    drive::DriveIntegrationService* const drive_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
    if (!drive_service || !drive_service->IsMounted()) {
      continue;
    }

    base::FilePath root("/");
    base::FilePath path = drive_service->GetMountPointPath();
    if (!root.AppendRelativePath(item->path, &path)) {
      continue;
    }

    results.emplace_back(FileSuggestionType::kDriveFile, path, justification,
                         absl::nullopt);
  }

  on_drive_results_ready_callback_list_.Notify(results);
}

}  // namespace ash
