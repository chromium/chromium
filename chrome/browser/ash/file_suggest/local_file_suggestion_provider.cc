// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/local_file_suggestion_provider.h"

#include <optional>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/files/justifications.h"
#include "chrome/browser/ash/app_list/search/ranking/util.h"
#include "chrome/browser/ash/app_list/search/util/mrfu_cache.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier_factory.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/ash/file_suggest/file_suggestion_provider.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

namespace {
constexpr base::TimeDelta kSaveDelay = base::Seconds(3);
constexpr base::TimeDelta kSuggestionNotificationDebounce =
    base::Milliseconds(100);

// Given the output of MrfuCache::GetAll, partition files into valid and invalid
// files. Valid files are files that:
// - Exist on-disk
// - Have been modified in the last |max_last_modified_time| days
std::pair<std::vector<LocalFileSuggestionProvider::LocalFileData>,
          std::vector<base::FilePath>>
ValidateFiles(const std::vector<std::pair<std::string, float>>& ranker_results,
              const base::TimeDelta& max_last_modified_time,
              std::vector<base::FilePath> trash_paths) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::vector<LocalFileSuggestionProvider::LocalFileData> valid_results;
  std::vector<base::FilePath> invalid_results;
  const base::Time now = base::Time::Now();
  for (const auto& path_score : ranker_results) {
    // We use FilePath::FromUTF8Unsafe to decode the filepath string. As per its
    // documentation, this is a safe use of the function because
    // LocalFileSuggestionProvider is only used on ChromeOS, for which filepaths
    // are UTF8.
    const auto& path = base::FilePath::FromUTF8Unsafe(path_score.first);

    // Exclude any paths that are parented at an enabled trash location.
    if (base::ranges::any_of(trash_paths,
                             [&path](const base::FilePath& trash_path) {
                               return trash_path.IsParent(path);
                             })) {
      invalid_results.emplace_back(path);
      continue;
    }

    base::File::Info info;
    if (base::PathExists(path) && base::GetFileInfo(path, &info) &&
        (now - info.last_modified <= max_last_modified_time)) {
      valid_results.emplace_back(LocalFileSuggestionProvider::LocalFileData{
          path_score.second, path, info});
    } else {
      invalid_results.emplace_back(path);
    }
  }
  return {valid_results, invalid_results};
}

}  // anonymous namespace

LocalFileSuggestionProvider::LocalFileSuggestionProvider(
    Profile* profile,
    base::RepeatingCallback<void(FileSuggestionType)> notify_update_callback)
    : FileSuggestionProvider(notify_update_callback),
      profile_(profile),
      max_last_modified_time_(GetMaxFileSuggestionRecency()) {
  DCHECK(profile_);

  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  auto* notifier =
      file_manager::file_tasks::FileTasksNotifierFactory::GetForProfile(
          profile);

  if (notifier) {
    file_tasks_observer_.Observe(notifier);

    app_list::MrfuCache::Params params;
    // 5 consecutive clicks to get a new file to a score of 0.8, and 10 clicks
    // on other files to reduce its score by half.
    params.half_life = 10.0f;
    params.boost_factor = 5.0f;
    app_list::MrfuCache::Proto proto(
        app_list::RankerStateDirectory(profile).AppendASCII(
            "zero_state_local_files.pb"),
        kSaveDelay);

    // `proto` is owned by `files_ranker_` which is a class member so it is safe
    // to call `RegisterOnInitUnsafe()`.
    proto.RegisterOnInitUnsafe(
        base::BindOnce(&LocalFileSuggestionProvider::OnProtoInitialized,
                       base::Unretained(this)));

    files_ranker_ =
        std::make_unique<app_list::MrfuCache>(std::move(proto), params);
  }
}

LocalFileSuggestionProvider::~LocalFileSuggestionProvider() = default;

bool LocalFileSuggestionProvider::IsInitialized() const {
  return files_ranker_ && files_ranker_->initialized();
}

void LocalFileSuggestionProvider::GetSuggestFileData(
    GetSuggestFileDataCallback callback) {
  if (!files_ranker_ || !files_ranker_->initialized()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (!on_validation_complete_callback_list_.empty()) {
    on_validation_complete_callback_list_.AddUnsafe(std::move(callback));
    return;
  }

  on_validation_complete_callback_list_.AddUnsafe(std::move(callback));

  // Generate the trash paths on the first get suggestion of file data. This is
  // to enable unit tests to mock out the trash paths appropriately.
  if (trash_paths_.empty()) {
    auto enabled_trash_locations =
        file_manager::trash::GenerateEnabledTrashLocationsForProfile(
            profile_, /*base_path=*/base::FilePath());
    for (const auto& it : enabled_trash_locations) {
      trash_paths_.emplace_back(
          it.first.Append(it.second.relative_folder_path));
    }
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ValidateFiles, files_ranker_->GetAll(),
                     max_last_modified_time_,
                     (file_manager::trash::IsTrashEnabledForProfile(profile_)
                          ? trash_paths_
                          : std::vector<base::FilePath>())),
      base::BindOnce(&LocalFileSuggestionProvider::OnValidationComplete,
                     weak_factory_.GetWeakPtr()));
}

void LocalFileSuggestionProvider::MaybeUpdateItemSuggestCache(
    base::PassKey<FileSuggestKeyedService>) {
  NOTREACHED_IN_MIGRATION();
}

void LocalFileSuggestionProvider::OnFilesOpened(
    const std::vector<FileOpenEvent>& file_opens) {
  if (!files_ranker_) {
    return;
  }

  const auto& profile_path = profile_->GetPath();
  for (const auto& file_open : file_opens) {
    // Filter out file opens if:
    // 1. The open event is not a kLaunch or a kOpen.
    if (file_open.open_type != FileTasksObserver::OpenType::kLaunch &&
        file_open.open_type != FileTasksObserver::OpenType::kOpen) {
      continue;
    }

    // 2. The open relates to a Drive file, which is handled by another
    // provider. Filter this out by checking if the file resides in the user's
    // cryptohome.
    if (!profile_path.IsParent(file_open.path) &&
        !file_manager::util::GetMyFilesFolderForProfile(profile_).IsParent(
            file_open.path) &&
        !file_manager::util::GetDownloadsFolderForProfile(profile_).IsParent(
            file_open.path)) {
      continue;
    }

    files_ranker_->Use(file_open.path.value());
  }

  if (!queued_notification_.IsRunning()) {
    queued_notification_.Start(
        FROM_HERE, kSuggestionNotificationDebounce,
        base::BindOnce(&LocalFileSuggestionProvider::NotifySuggestionUpdate,
                       weak_factory_.GetWeakPtr(),
                       FileSuggestionType::kLocalFile));
  }
}

void LocalFileSuggestionProvider::OnProtoInitialized() {
  NotifySuggestionUpdate(FileSuggestionType::kLocalFile);
}

void LocalFileSuggestionProvider::OnValidationComplete(
    std::pair<std::vector<LocalFileData>, std::vector<base::FilePath>>
        results) {
  // Delete invalid results from the ranker.
  for (const base::FilePath& path : results.second) {
    files_ranker_->Delete(path.value());
  }

  std::vector<FileSuggestData> final_results;
  for (auto& result : results.first) {
    std::optional<std::u16string> justification_string;
    if (result.info.last_accessed > result.info.last_modified) {
      justification_string = app_list::GetJustificationString(
          FileSuggestionJustificationType::kViewed, result.info.last_accessed,
          /*user_name=*/"");
    } else {
      justification_string = app_list::GetJustificationString(
          FileSuggestionJustificationType::kModifiedByCurrentUser,
          result.info.last_modified,
          /*user_name=*/"");
    }

    final_results.emplace_back(FileSuggestionType::kLocalFile, result.path,
                               /*title=*/std::nullopt, justification_string,
                               /*modified_time=*/result.info.last_modified,
                               /*viewed_time=*/result.info.last_accessed,
                               /*shared_time=*/std::nullopt, result.score,
                               /*drive_file_id=*/std::nullopt,
                               /*icon_url=*/std::nullopt);
  }

  // Sort valid results high-to-low by score.
  std::sort(final_results.begin(), final_results.end(),
            [](const auto& a, const auto& b) {
              return a.score.value() > b.score.value();
            });

  on_validation_complete_callback_list_.Notify(final_results);
  DCHECK(on_validation_complete_callback_list_.empty());
}

}  // namespace ash
