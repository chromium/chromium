// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SUGGEST_DRIVE_FILE_SUGGESTION_PROVIDER_H_
#define CHROME_BROWSER_ASH_FILE_SUGGEST_DRIVE_FILE_SUGGESTION_PROVIDER_H_

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/file_suggest/file_suggestion_provider.h"
#include "chrome/browser/ash/file_suggest/item_suggest_cache.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"

namespace drive {
class DriveIntegrationService;
}  // namespace drive

namespace ash {
enum class DriveSuggestValidationStatus;
class FileSuggestKeyedService;

// A suggestion provider that handles the drive file suggestions.
class DriveFileSuggestionProvider : public FileSuggestionProvider {
 public:
  DriveFileSuggestionProvider(
      Profile* profile,
      base::RepeatingCallback<void(FileSuggestionType)> notify_update_callback);
  DriveFileSuggestionProvider(const DriveFileSuggestionProvider&) = delete;
  DriveFileSuggestionProvider& operator=(const DriveFileSuggestionProvider&) =
      delete;
  ~DriveFileSuggestionProvider() override;

  // FileSuggestionProvider:
  void GetSuggestFileData(GetSuggestFileDataCallback callback) override;
  void MaybeUpdateItemSuggestCache(
      base::PassKey<FileSuggestKeyedService>) override;

  ItemSuggestCache* item_suggest_cache_for_test() {
    return item_suggest_cache_.get();
  }

 private:
  // Called when locating drive files through the drive service is completed.
  // Returns the location result through `paths`. `raw_suggest_results` is the
  // file suggestion data before validation.
  void OnDriveFilePathsLocated(
      std::vector<ItemSuggestCache::Result> raw_suggest_results,
      std::optional<std::vector<drivefs::mojom::FilePathOrErrorPtr>> paths);

  // Ends the validation on drive suggestion file paths and publishes the
  // result.
  void EndDriveFilePathValidation(
      DriveSuggestValidationStatus validation_status,
      const std::optional<std::vector<FileSuggestData>>& suggest_results);

  const raw_ptr<Profile> profile_;

  const raw_ptr<drive::DriveIntegrationService> drive_service_;

  // The drive client from which the raw suggest data (i.e. the data before
  // validation) is fetched.
  std::unique_ptr<ItemSuggestCache> item_suggest_cache_;

  // Guards the callback registered on `item_suggest_cache_`.
  base::CallbackListSubscription item_suggest_subscription_;

  // The callbacks that run when the drive suggest results are ready.
  // Use a callback list to handle the edge case that multiple data consumers
  // wait for the drive suggest results.
  base::OnceCallbackList<GetSuggestFileDataCallback::RunType>
      on_drive_results_ready_callback_list_;

  // A drive file needs to have been modified more recently than this to be
  // considered valid.
  const base::TimeDelta drive_file_max_last_modified_time_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to post the task to filter drive suggestion results.
  scoped_refptr<base::SequencedTaskRunner> result_filter_task_runner_;

  // Used to guard the calling to get drive suggestion results.
  base::WeakPtrFactory<DriveFileSuggestionProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SUGGEST_DRIVE_FILE_SUGGESTION_PROVIDER_H_
