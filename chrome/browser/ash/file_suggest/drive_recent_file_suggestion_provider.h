// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SUGGEST_DRIVE_RECENT_FILE_SUGGESTION_PROVIDER_H_
#define CHROME_BROWSER_ASH_FILE_SUGGEST_DRIVE_RECENT_FILE_SUGGESTION_PROVIDER_H_

#include <vector>

#include "base/callback_list.h"
#include "chrome/browser/ash/file_suggest/file_suggestion_provider.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"

class Profile;

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
  // Callback for a Drive FS search query.
  void OnSearchDriveFs(
      drive::FileError error,
      absl::optional<std::vector<drivefs::mojom::QueryItemPtr>> items);

  const raw_ptr<Profile> profile_;

  // The callbacks that run when the drive results are ready.
  // Using a callback list to handle the edge case that multiple data consumers
  // wait for the drive results.
  base::OnceCallbackList<GetSuggestFileDataCallback::RunType>
      on_drive_results_ready_callback_list_;

  // Used to guard the calling to get drive suggestion results.
  base::WeakPtrFactory<DriveRecentFileSuggestionProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SUGGEST_DRIVE_RECENT_FILE_SUGGESTION_PROVIDER_H_
