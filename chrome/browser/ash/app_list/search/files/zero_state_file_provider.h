// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_ZERO_STATE_FILE_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_ZERO_STATE_FILE_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier.h"
#include "chrome/browser/ash/file_manager/file_tasks_observer.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ui/ash/thumbnail_loader/thumbnail_loader.h"

class Profile;

namespace app_list {

// ZeroStateFileProvider recommends local files from a cache. Files are added to
// the cache whenever they are opened.
class ZeroStateFileProvider : public SearchProvider,
                              public ash::FileSuggestKeyedService::Observer {
 public:
  explicit ZeroStateFileProvider(Profile* profile);

  ZeroStateFileProvider(const ZeroStateFileProvider&) = delete;
  ZeroStateFileProvider& operator=(const ZeroStateFileProvider&) = delete;

  ~ZeroStateFileProvider() override;

  // SearchProvider:
  void StartZeroState() override;
  void StopZeroState() override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  // Called when file suggestion data are fetched from the service.
  void OnSuggestFileDataFetched(
      const std::optional<std::vector<ash::FileSuggestData>>& suggest_results);

  // Builds the search results from file suggestions then publishes the results.
  void SetSearchResults(
      const std::vector<ash::FileSuggestData>& suggest_results);

  // TODO(crbug.com/1349618): Remove this once the Continue tast test does not
  // rely on it.
  void AppendFakeSearchResults(Results* results);

  // FileSuggestKeyedService::Observer:
  void OnFileSuggestionUpdated(ash::FileSuggestionType type) override;

  // The reference to profile to get ZeroStateFileProvider service.
  const raw_ptr<Profile> profile_;

  ash::ThumbnailLoader thumbnail_loader_;

  const raw_ptr<ash::FileSuggestKeyedService> file_suggest_service_;

  base::TimeTicks query_start_time_;

  // Path to the downloads folder for this profile.
  const base::FilePath downloads_path_;

  base::ScopedObservation<ash::FileSuggestKeyedService,
                          ash::FileSuggestKeyedService::Observer>
      file_suggest_service_observation_{this};

  base::WeakPtrFactory<ZeroStateFileProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_ZERO_STATE_FILE_PROVIDER_H_
