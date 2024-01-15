// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SUGGEST_DRIVE_RECENT_FILE_SUGGESTION_PROVIDER_H_
#define CHROME_BROWSER_ASH_FILE_SUGGEST_DRIVE_RECENT_FILE_SUGGESTION_PROVIDER_H_

#include <map>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/file_suggest/file_suggestion_provider.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"

class Profile;

namespace drive {
class DriveIntegrationService;
}

namespace ash {
class FileSuggestKeyedService;

// A suggestion provider for most recently used drive files.
class DriveRecentFileSuggestionProvider : public FileSuggestionProvider {
 public:
  DriveRecentFileSuggestionProvider(
      Profile* profile,
      base::RepeatingCallback<void(FileSuggestionType)> notify_update_callback);
  DriveRecentFileSuggestionProvider(const DriveRecentFileSuggestionProvider&) =
      delete;
  DriveRecentFileSuggestionProvider& operator=(
      const DriveRecentFileSuggestionProvider&) = delete;
  ~DriveRecentFileSuggestionProvider() override;

  // FileSuggestionProvider:
  void GetSuggestFileData(GetSuggestFileDataCallback callback) override;
  void MaybeUpdateItemSuggestCache(
      base::PassKey<FileSuggestKeyedService>) override;

 private:
  // Runs Drive FS search using the provided query parameters.
  // `callback` gets run when the search completes. The search results are added
  // to `pending_results_`.
  void PerformSearch(drivefs::mojom::QueryParametersPtr query,
                     drive::DriveIntegrationService* drive_service,
                     base::RepeatingClosure callback);

  // Run upon completion of all Drive FS searches - includes searched for
  // recently modified files, and files recently viewd by the user. It
  // aggregates results, and runs callbacks waiting for file suggestions.
  void OnRecentFilesSearchesCompleted();

  // Callback for a single Drive FS search query. Saves the returned results in
  // `pending_results_`, and runs `callback`.
  void OnSearchRequestComplete(
      base::RepeatingClosure callback,
      drive::FileError error,
      std::optional<std::vector<drivefs::mojom::QueryItemPtr>> items);

  const raw_ptr<Profile> profile_;

  // The callbacks that run when the drive results are ready.
  // Using a callback list to handle the edge case that multiple data consumers
  // wait for the drive results.
  base::OnceCallbackList<GetSuggestFileDataCallback::RunType>
      on_drive_results_ready_callback_list_;

  // Keeps track of results returned by individual Drive FS searches.
  std::map<base::FilePath, drivefs::mojom::FileMetadataPtr>
      query_result_files_by_path_;

  // Used to guard the calling to get drive suggestion results.
  base::WeakPtrFactory<DriveRecentFileSuggestionProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SUGGEST_DRIVE_RECENT_FILE_SUGGESTION_PROVIDER_H_
