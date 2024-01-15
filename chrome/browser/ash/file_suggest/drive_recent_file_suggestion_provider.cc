// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/drive_recent_file_suggestion_provider.h"

#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/ranges/algorithm.h"
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
  return query;
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
    const base::Time& modified_time = item.second->modification_time;
    const base::Time& viewed_time = item.second->last_viewed_by_me_time;

    const bool is_view_more_recent = viewed_time > modified_time;

    const base::Time timestamp =
        is_view_more_recent ? viewed_time : modified_time;
    // TODO(b/316180378): Localize justification strings.
    const std::u16string action_string = is_view_more_recent
                                             ? u"[Needs i18n] You viewed"
                                             : u"[Needs i18n] Modified";
    const std::u16string relative_date =
        ui::TimeFormat::RelativeDate(timestamp, nullptr);
    std::u16string justification;
    if (!relative_date.empty()) {
      justification = base::JoinString(
          {action_string, base::ToLowerASCII(relative_date)}, u" ");
    } else {
      justification = base::JoinString(
          {action_string, base::TimeFormatShortDate(timestamp)}, u" ");
    }

    base::FilePath root("/");
    base::FilePath path = drive_service->GetMountPointPath();
    if (!root.AppendRelativePath(item.first, &path)) {
      continue;
    }

    results.emplace_back(FileSuggestionType::kDriveFile, path, justification,
                         timestamp, /*new_score=*/std::nullopt);
  }

  query_result_files_by_path_.clear();

  base::ranges::sort(results, base::ranges::greater(),
                     &ash::FileSuggestData::timestamp);

  on_drive_results_ready_callback_list_.Notify(results);
}

}  // namespace ash
