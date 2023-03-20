// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_DRIVE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_DRIVE_SEARCH_PROVIDER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "components/drive/file_errors.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace drive {
class DriveIntegrationService;
}  // namespace drive

namespace app_list {

class DriveSearchProvider : public SearchProvider {
 public:
  struct FileInfo {
    base::FilePath reparented_path;
    drivefs::mojom::FileMetadataPtr metadata;
    absl::optional<base::Time> last_accessed;

    FileInfo(const base::FilePath& reparented_path,
             drivefs::mojom::FileMetadataPtr metadata,
             const absl::optional<base::Time>& last_accessed);
    ~FileInfo();

    FileInfo(const FileInfo&) = delete;
    FileInfo& operator=(const FileInfo&) = delete;
  };

  explicit DriveSearchProvider(Profile* profile);
  ~DriveSearchProvider() override;

  DriveSearchProvider(const DriveSearchProvider&) = delete;
  DriveSearchProvider& operator=(const DriveSearchProvider&) = delete;

  // SearchProvider:
  ash::AppListSearchResultType ResultType() const override;
  void Start(const std::u16string& query) override;
  void StopQuery() override;

 private:
  void OnSearchDriveByFileName(drive::FileError error,
                               std::vector<drivefs::mojom::QueryItemPtr> items);
  void SetSearchResults(std::vector<std::unique_ptr<FileInfo>> items);
  std::unique_ptr<FileResult> MakeResult(
      const base::FilePath& path,
      double relevance,
      FileResult::Type type,
      const absl::optional<std::string>& drive_id);

  // When the query began.
  base::TimeTicks query_start_time_;
  // When DriveFS returned results for the query.
  base::TimeTicks results_returned_time_;

  std::u16string last_query_;
  absl::optional<ash::string_matching::TokenizedString> last_tokenized_query_;

  Profile* const profile_;
  drive::DriveIntegrationService* const drive_service_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DriveSearchProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_DRIVE_SEARCH_PROVIDER_H_
