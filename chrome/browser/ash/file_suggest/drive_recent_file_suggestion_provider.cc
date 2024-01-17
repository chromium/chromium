// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/drive_recent_file_suggestion_provider.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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

constexpr base::TimeDelta kMaxLastModifiedOrViewedTime = base::Days(8);

drivefs::mojom::QueryParametersPtr CreateRecentlyModifiedQuery() {
  auto query = drivefs::mojom::QueryParameters::New();
  query->modified_time = base::Time::Now() - kMaxLastModifiedOrViewedTime;
  query->modified_time_operator =
      drivefs::mojom::QueryParameters::DateComparisonOperator::kGreaterThan;
  query->page_size = 10;
  query->query_source =
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
  query->sort_direction =
      drivefs::mojom::QueryParameters::SortDirection::kDescending;
  query->sort_field = drivefs::mojom::QueryParameters::SortField::kLastModified;
  return query;
}

drivefs::mojom::QueryParametersPtr CreateRecentlyViewedQuery() {
  auto query = drivefs::mojom::QueryParameters::New();
  query->page_size = 10;
  query->query_source =
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
  query->sort_direction =
      drivefs::mojom::QueryParameters::SortDirection::kDescending;
  query->sort_field =
      drivefs::mojom::QueryParameters::SortField::kLastViewedByMe;
  query->viewed_time = base::Time::Now() - kMaxLastModifiedOrViewedTime;
  query->viewed_time_operator =
      drivefs::mojom::QueryParameters::DateComparisonOperator::kGreaterThan;
  return query;
}

std::u16string GetDateString(const base::Time& timestamp) {
  const std::u16string relative_date =
      ui::TimeFormat::RelativeDate(timestamp, nullptr);
  if (!relative_date.empty()) {
    return base::ToLowerASCII(relative_date);
  }
  return base::TimeFormatShortDate(timestamp);
}

FileSuggestData CreateFileSuggestion(
    const base::FilePath& path,
    const drivefs::mojom::FileMetadata& file_metadata) {
  const base::Time& modified_time = file_metadata.modification_time;
  const base::Time& viewed_time = file_metadata.last_viewed_by_me_time;

  // If the file was shared with user, but not yet viewed by the user, surface
  // it as a shared file.
  if (const absl::optional<base::Time>& shared_time =
          file_metadata.shared_with_me_time;
      shared_time && !shared_time->is_null() && viewed_time.is_null()) {
    if (file_metadata.sharing_user &&
        features::IsShowSharingUserInLauncherContinueSectionEnabled()) {
      return FileSuggestData(
          FileSuggestionType::kDriveFile, path,
          base::JoinString(
              {u"[Needs i18n]",
               base::UTF8ToUTF16(file_metadata.sharing_user->display_name),
               u"shared with you", GetDateString(*shared_time)},
              u" "),
          shared_time, /*new_score=*/absl::nullopt);
    }
    return FileSuggestData(FileSuggestionType::kDriveFile, path,
                           base::JoinString({u"[Needs i18n] Shared with you",
                                             GetDateString(*shared_time)},
                                            u" "),
                           *shared_time, /*new_score=*/absl::nullopt);
  }

  // Viewed by the user more recently than the last modification.
  if (viewed_time > modified_time) {
    return FileSuggestData(
        FileSuggestionType::kDriveFile, path,
        base::JoinString(
            {u"[Needs i18n] You viewed", GetDateString(viewed_time)}, u" "),
        viewed_time, /*new_score=*/absl::nullopt);
  }

  // Last modification was by the user.
  if (file_metadata.modified_by_me_time >= modified_time) {
    return FileSuggestData(
        FileSuggestionType::kDriveFile, path,
        base::JoinString({u"[Needs i18n] You modified",
                          GetDateString(*file_metadata.modified_by_me_time)},
                         u" "),
        *file_metadata.modified_by_me_time, /*new_score=*/absl::nullopt);
  }

  // Last modification was by another user - surface the last momdifying user
  // name.
  if (file_metadata.last_modifying_user) {
    return FileSuggestData(
        FileSuggestionType::kDriveFile, path,
        base::JoinString(
            {u"[Needs i18n]",
             base::UTF8ToUTF16(file_metadata.last_modifying_user->display_name),
             u"modified", GetDateString(modified_time)},
            u" "),
        modified_time, /*new_score=*/absl::nullopt);
  }

  // Fallback string when the last modifying user is unknown.
  return FileSuggestData(
      FileSuggestionType::kDriveFile, path,
      base::JoinString({u"[Needs i18n] Modified", GetDateString(modified_time)},
                       u" "),
      modified_time, /*new_score=*/absl::nullopt);
}

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

  base::RepeatingClosure search_callback = base::BarrierClosure(
      2, base::BindOnce(
             &DriveRecentFileSuggestionProvider::OnRecentFilesSearchesCompleted,
             weak_factory_.GetWeakPtr()));
  PerformSearch(CreateRecentlyModifiedQuery(), drive_service, search_callback);
  PerformSearch(CreateRecentlyViewedQuery(), drive_service, search_callback);
}

void DriveRecentFileSuggestionProvider::PerformSearch(
    drivefs::mojom::QueryParametersPtr query,
    drive::DriveIntegrationService* drive_service,
    base::RepeatingClosure callback) {
  drive_service->GetDriveFsHost()->PerformSearch(
      std::move(query),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &DriveRecentFileSuggestionProvider::OnSearchRequestComplete,
              weak_factory_.GetWeakPtr(), std::move(callback)),
          drive::FileError::FILE_ERROR_ABORT, /*items=*/std::nullopt));
}

void DriveRecentFileSuggestionProvider::MaybeUpdateItemSuggestCache(
    base::PassKey<FileSuggestKeyedService>) {}

void DriveRecentFileSuggestionProvider::OnSearchRequestComplete(
    base::RepeatingClosure callback,
    drive::FileError error,
    std::optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  if (error == drive::FileError::FILE_ERROR_OK && items) {
    for (auto& item : *items) {
      query_result_files_by_path_.emplace(item->path,
                                          std::move(item->metadata));
    }
  }
  callback.Run();
}

void DriveRecentFileSuggestionProvider::OnRecentFilesSearchesCompleted() {
  std::vector<FileSuggestData> results;

  drive::DriveIntegrationService* const drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (!drive_service || !drive_service->IsMounted()) {
    query_result_files_by_path_.clear();
    on_drive_results_ready_callback_list_.Notify(results);
    return;
  }

  for (const auto& item : query_result_files_by_path_) {
    base::FilePath root("/");
    base::FilePath path = drive_service->GetMountPointPath();
    if (!root.AppendRelativePath(item.first, &path)) {
      continue;
    }

    results.push_back(CreateFileSuggestion(path, *item.second));
  }

  query_result_files_by_path_.clear();

  base::ranges::sort(results, base::ranges::greater(),
                     &FileSuggestData::timestamp);

  on_drive_results_ready_callback_list_.Notify(results);
}

}  // namespace ash
