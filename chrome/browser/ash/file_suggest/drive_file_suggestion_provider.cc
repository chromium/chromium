// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/drive_file_suggestion_provider.h"

#include "ash/constants/ash_pref_names.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"

namespace ash {
namespace {

// The minimum number of results required to keep using the short delay. This
// means that results are refreshed more often if there are enough high-quality
// results returned.
constexpr size_t kShortDelayQuota = 3u;
constexpr base::TimeDelta kMaxLastModifiedTime = base::Days(8);

// Given an absolute path representing a file in the user's Drive, returns a
// reparented version of the path within the user's drive fs mount.
base::FilePath ReparentToDriveMount(
    const base::FilePath& path,
    const drive::DriveIntegrationService* drive_service) {
  DCHECK(!path.IsAbsolute());
  return drive_service->GetMountPointPath().Append(path.value());
}

// Sets on the specified profile whether to use a long delay duration in the
// query for drive file suggest data.
void SetUseLongDelayInDriveSuggestQuery(Profile* profile, bool use_long_delay) {
  profile->GetPrefs()->SetBoolean(ash::prefs::kLauncherUseLongContinueDelay,
                                  use_long_delay);
}

// Filters out the files that exceed the max last modified time.
std::vector<FileSuggestData> FilterSuggestResultsByTime(
    std::vector<FileSuggestData> suggest_results,
    base::TimeDelta max_last_modified_time) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::vector<FileSuggestData> filtered_results;
  const base::Time now = base::Time::Now();
  for (const auto& suggest_result : suggest_results) {
    base::File::Info info;
    const auto& path = suggest_result.file_path;
    if (!base::PathExists(path) || !base::GetFileInfo(path, &info) ||
        now - info.last_modified > max_last_modified_time) {
      // Filter out this suggestion data.
      continue;
    }

    filtered_results.push_back(suggest_result);
  }

  return filtered_results;
}

}  // namespace

DriveFileSuggestionProvider::DriveFileSuggestionProvider(
    Profile* profile,
    base::RepeatingCallback<void(FileSuggestionType)> notify_update_callback)
    : FileSuggestionProvider(notify_update_callback),
      profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetInstance()->GetForProfile(
              profile_)),
      item_suggest_cache_(std::make_unique<ItemSuggestCache>(
          g_browser_process->GetApplicationLocale(),
          profile,
          profile->GetDefaultStoragePartition()
              ->GetURLLoaderFactoryForBrowserProcess())),
      drive_file_max_last_modified_time_(kMaxLastModifiedTime),
      result_filter_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // It's safe to use Unretained(this) by contract of the
  // CallbackListSubscription.
  item_suggest_subscription_ =
      item_suggest_cache_->RegisterCallback(base::BindRepeating(
          &DriveFileSuggestionProvider::NotifySuggestionUpdate,
          base::Unretained(this), FileSuggestionType::kDriveFile));
}

DriveFileSuggestionProvider::~DriveFileSuggestionProvider() = default;

void DriveFileSuggestionProvider::GetSuggestFileData(
    GetSuggestFileDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool has_active_validation =
      !on_drive_results_ready_callback_list_.empty();

  // Add `callback` to the waiting list.
  on_drive_results_ready_callback_list_.AddUnsafe(std::move(callback));

  // Return early if there is an active validation. `callback` will run when
  // validation completes.
  if (has_active_validation) {
    return;
  }

  // If there is not any available drive service, return early.
  if (!drive_service_ || !drive_service_->IsMounted()) {
    EndDriveFilePathValidation(DriveSuggestValidationStatus::kDriveFSNotMounted,
                               /*suggest_results=*/std::nullopt);
    return;
  } else if (profile_->GetPrefs()->GetBoolean(drive::prefs::kDisableDrive)) {
    EndDriveFilePathValidation(DriveSuggestValidationStatus::kDriveDisabled,
                               /*suggest_results=*/std::nullopt);
    return;
  }

  const std::optional<ItemSuggestCache::Results> results_before_validation =
      item_suggest_cache_->GetResults();

  // If there is no available data to validate, return early.
  if (!results_before_validation ||
      results_before_validation->results.empty()) {
    // An empty but non-null value indicates that the cache was updated
    // successfully, and no results were returned.
    if (results_before_validation &&
        results_before_validation->results.empty()) {
      SetUseLongDelayInDriveSuggestQuery(profile_, true);
    }

    EndDriveFilePathValidation(DriveSuggestValidationStatus::kNoResults,
                               /*suggest_results=*/std::nullopt);
    return;
  }

  std::vector<std::string> item_ids;
  for (const auto& result : results_before_validation->results) {
    item_ids.push_back(result.id);
  }

  // Validate the drive cache results whenever drive file suggest data is
  // fetched to guarantee the file path validity.
  drive_service_->LocateFilesByItemIds(
      item_ids,
      base::BindOnce(&DriveFileSuggestionProvider::OnDriveFilePathsLocated,
                     weak_factory_.GetWeakPtr(),
                     results_before_validation->results));
}

void DriveFileSuggestionProvider::MaybeUpdateItemSuggestCache(
    base::PassKey<FileSuggestKeyedService>) {
  item_suggest_cache_->MaybeUpdateCache();
}

void DriveFileSuggestionProvider::EndDriveFilePathValidation(
    DriveSuggestValidationStatus validation_status,
    const std::optional<std::vector<FileSuggestData>>& suggest_results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If there aren't enough results, use a long delay and vice versa.
  if (validation_status == DriveSuggestValidationStatus::kOk) {
    SetUseLongDelayInDriveSuggestQuery(
        profile_, suggest_results->size() < kShortDelayQuota);
  }

  // TODO(https://crbug.com/1356344): when the refactoring code is stable,
  // remove the obsolete histogram originally used by `ZeroStateDriveProvider`.
  base::UmaHistogramEnumeration(
      "Ash.Search.DriveFileSuggestDataValidation.Status", validation_status);

  // Notify observers of the fetched drive suggest results.
  on_drive_results_ready_callback_list_.Notify(suggest_results);
  DCHECK(on_drive_results_ready_callback_list_.empty());
}

void DriveFileSuggestionProvider::OnDriveFilePathsLocated(
    std::vector<ItemSuggestCache::Result> raw_suggest_results,
    std::optional<std::vector<drivefs::mojom::FilePathOrErrorPtr>> paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If validation fails, return early.
  if (!paths) {
    EndDriveFilePathValidation(
        DriveSuggestValidationStatus::kPathLocationFailed,
        /*suggest_results=*/std::nullopt);
    return;
  }

  DCHECK_EQ(raw_suggest_results.size(), paths->size());

  std::vector<FileSuggestData> suggest_results;
  for (size_t index = 0; index < paths->size(); ++index) {
    const auto& path_or_error = paths->at(index);

    // Fail to validate the file path at `index`, so skip this loop iteration.
    if (!path_or_error->is_path()) {
      continue;
    }

    std::optional<std::u16string> reason;
    if (raw_suggest_results[index].prediction_reason) {
      reason = base::UTF8ToUTF16(
          raw_suggest_results[index].prediction_reason.value());
    }
    suggest_results.emplace_back(
        FileSuggestionType::kDriveFile,
        ReparentToDriveMount(path_or_error->get_path(), drive_service_),
        /*title=*/std::nullopt, reason,
        /*modified_time=*/std::nullopt,
        /*viewed_time=*/std::nullopt,
        /*shared_time=*/std::nullopt,
        /*score=*/std::nullopt,
        /*drive_file_id=*/std::nullopt,
        /*icon_url=*/std::nullopt);
  }

  // Validation fails on each file, so return early.
  if (suggest_results.empty()) {
    EndDriveFilePathValidation(DriveSuggestValidationStatus::kAllFilesErrored,
                               /*suggest_results=*/std::nullopt);
    return;
  }

  result_filter_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FilterSuggestResultsByTime, suggest_results,
                     drive_file_max_last_modified_time_),
      base::BindOnce(&DriveFileSuggestionProvider::EndDriveFilePathValidation,
                     weak_factory_.GetWeakPtr(),
                     DriveSuggestValidationStatus::kOk));
}

}  // namespace ash
