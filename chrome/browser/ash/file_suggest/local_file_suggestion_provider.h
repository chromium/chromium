// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SUGGEST_LOCAL_FILE_SUGGESTION_PROVIDER_H_
#define CHROME_BROWSER_ASH_FILE_SUGGEST_LOCAL_FILE_SUGGESTION_PROVIDER_H_

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/util/mrfu_cache.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier.h"
#include "chrome/browser/ash/file_manager/file_tasks_observer.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/ash/file_suggest/file_suggestion_provider.h"

class Profile;

namespace ash {

// This is the provider for local file suggestions.
// Currently stubbed.
class LocalFileSuggestionProvider
    : public FileSuggestionProvider,
      public file_manager::file_tasks::FileTasksObserver {
 public:
  struct LocalFileData {
    float score;
    base::FilePath path;
    base::File::Info info;
  };

  LocalFileSuggestionProvider(
      Profile* profile,
      base::RepeatingCallback<void(FileSuggestionType)> notify_update_callback);
  LocalFileSuggestionProvider(const LocalFileSuggestionProvider&) = delete;
  LocalFileSuggestionProvider& operator=(const LocalFileSuggestionProvider&) =
      delete;
  ~LocalFileSuggestionProvider() override;

  // Returns true if the MrfuCache is initialized.
  bool IsInitialized() const;

  // FileSuggestionProvider:
  void GetSuggestFileData(GetSuggestFileDataCallback callback) override;
  void MaybeUpdateItemSuggestCache(
      base::PassKey<FileSuggestKeyedService>) override;

  // file_manager::file_tasks::FileTaskObserver:
  void OnFilesOpened(const std::vector<FileOpenEvent>& file_opens) override;


 private:
  void OnProtoInitialized();
  void OnValidationComplete(std::pair<std::vector<LocalFileData>,
                                      std::vector<base::FilePath>> results);

  const raw_ptr<Profile> profile_;

  // Any file not modified at least as recently as `max_last_modified_time_` ago
  // will be filtered out of results.
  const base::TimeDelta max_last_modified_time_;

  std::unique_ptr<app_list::MrfuCache> files_ranker_;

  // After a file is opened, if this timer is not running, we set it to run for
  // a brief delay and then call `NotifySuggestionUpdate()`. This debounces file
  // open events to prevent us from calling it many times instantly when many
  // files are opened at once.
  base::OneShotTimer queued_notification_;

  // Callbacks awaiting validation completion. This prevents issues in the event
  // that multiple clients request results simultaneously.
  base::OnceCallbackList<GetSuggestFileDataCallback::RunType>
      on_validation_complete_callback_list_;

  base::ScopedObservation<file_manager::file_tasks::FileTasksNotifier,
                          file_manager::file_tasks::FileTasksObserver>
      file_tasks_observer_{this};

  // A list of paths that represent absolute locations for enabled trash folders
  // on the users system. This list is only used if the Trash feature and
  // enterprise policy are enabled.
  std::vector<base::FilePath> trash_paths_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<LocalFileSuggestionProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SUGGEST_LOCAL_FILE_SUGGESTION_PROVIDER_H_
