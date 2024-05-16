// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_RECENT_DRIVE_SOURCE_H_
#define CHROME_BROWSER_ASH_FILEAPI_RECENT_DRIVE_SOURCE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/id_map.h"
#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/fileapi/recent_source.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace ash {

class RecentFile;

// RecentSource implementation for Google Drive files.
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
  void GetRecentFiles(const Params& params,
                      GetRecentFilesCallback callback) override;

  // Overrides the Stop method to implement search interruption.
  std::vector<RecentFile> Stop(const int32_t call_id) override;

  // Generates type filters based on the file_type parameter. This is done so
  // that this code can be shared between recent files and file search.
  static std::vector<std::string> CreateTypeFilters(
      RecentSource::FileType file_type);

 private:
  static const char kLoadHistogramName[];

  // The context for a single GetRecentFiles call. Multiple, parallel calls can
  // be issued and each receives its unique context, stored in the map using the
  // call_id parameter.
  struct CallContext {
    explicit CallContext(GetRecentFilesCallback callback);
    // Move constructor necessary due to move-only callback type.
    CallContext(CallContext&& callcontext);
    ~CallContext();

    // The callback on which the results are delivered.
    GetRecentFilesCallback callback;

    // The time the GetRecentFiles request was started.
    base::TimeTicks build_start_time;

    // The list of files built up by the request.
    std::vector<RecentFile> files;

    // The mojo remote that performs Drive search.
    mojo::Remote<drivefs::mojom::SearchQuery> search_query;
  };

  void OnComplete(const int32_t call_id);

  void GotSearchResults(
      const Params& params,
      drive::FileError error,
      std::optional<std::vector<drivefs::mojom::QueryItemPtr>> results);

  // The current profile for which this source was constructed. This class does
  // not own the Profile object. Instead it uses it to fetch a drive integration
  // service for fetching matched files.
  const raw_ptr<Profile> profile_;

  // A map from call_id to the call context.
  base::IDMap<std::unique_ptr<CallContext>> context_map_;

  base::WeakPtrFactory<RecentDriveSource> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_RECENT_DRIVE_SOURCE_H_
