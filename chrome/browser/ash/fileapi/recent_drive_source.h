// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_RECENT_DRIVE_SOURCE_H_
#define CHROME_BROWSER_ASH_FILEAPI_RECENT_DRIVE_SOURCE_H_

#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/fileapi/recent_source.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace ash {

class RecentFile;

// RecentSource implementation for Drive files.
//
// All member functions must be called on the UI thread.
//
// TODO(nya): Write unit tests.
class RecentDriveSource : public RecentSource {
 public:
  explicit RecentDriveSource(Profile* profile);

  RecentDriveSource(const RecentDriveSource&) = delete;
  RecentDriveSource& operator=(const RecentDriveSource&) = delete;

  ~RecentDriveSource() override;

  // RecentSource overrides:
  void GetRecentFiles(Params params) override;

  // Generates type filters based on the file_type parameter. This is done so
  // that this code can be shared between recent files and file search.
  static std::vector<std::string> CreateTypeFilters(
      RecentSource::FileType file_type);

 private:
  static const char kLoadHistogramName[];

  void OnComplete();

  void GotSearchResults(
      drive::FileError error,
      absl::optional<std::vector<drivefs::mojom::QueryItemPtr>> results);

  Profile* const profile_;

  // Set at the beginning of GetRecentFiles().
  absl::optional<Params> params_;

  base::TimeTicks build_start_time_;

  std::vector<RecentFile> files_;

  mojo::Remote<drivefs::mojom::SearchQuery> search_query_;

  base::WeakPtrFactory<RecentDriveSource> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_RECENT_DRIVE_SOURCE_H_
