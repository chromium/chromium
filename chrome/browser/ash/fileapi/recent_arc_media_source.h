// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_RECENT_ARC_MEDIA_SOURCE_H_
#define CHROME_BROWSER_ASH_FILEAPI_RECENT_ARC_MEDIA_SOURCE_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "ash/components/arc/mojom/file_system.mojom.h"
#include "base/containers/id_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/ash/fileapi/recent_source.h"
#include "chrome/browser/profiles/profile.h"

class Profile;

namespace ash {

class RecentFile;

// RecentSource implementation for ARC media view. This class is not designed to
// be used by itself. Instead, it is instantiated and used by the RecentModel to
// retrieve recent files from ARC media view.
//
// All member functions must be called on the UI thread.
class RecentArcMediaSource : public RecentSource {
 public:
  // Creates a recent file sources that scans Arc media. The `profile` is used
  // to arc::ArcFileSystemOperationRunner and arc::ArcDocumentsProviderRootMap
  // for retrieving recent files and scanning ARC directories, respectively. The
  // `root_id` must be one of the know ARC root IDs, denoting Documents, Videos,
  // Images or Audio roots.
  RecentArcMediaSource(Profile* profile, const std::string& root_id);

  RecentArcMediaSource(const RecentArcMediaSource&) = delete;
  RecentArcMediaSource& operator=(const RecentArcMediaSource&) = delete;

  ~RecentArcMediaSource() override;

  // Overrides the base class method to launch searches for recent file in the
  // root identified by the `root_id` parameter given at the construction time.
  void GetRecentFiles(const Params& params,
                      GetRecentFilesCallback callback) override;

  // Overrides the base class Stop method to return partial results collected
  // before the timeout call. This method must be called on the UI thread.
  std::vector<RecentFile> Stop(const int32_t call_id) override;

  // Causes laggy performance for this source. This is to be only used in tests.
  void SetLagForTesting(const base::TimeDelta& lag);

  // The name of the metric under which recent file access statistics for ARC
  // are recorded.
  static const char kLoadHistogramName[];

 private:
  // Call context stores information specific to a single GetRecentFiles call.
  // If multiple calls are issued each will have its own context.
  struct CallContext {
    CallContext(const Params& params, GetRecentFilesCallback callback);
    // Move constructor needed as callback cannot be copied.
    CallContext(CallContext&& context);
    ~CallContext();

    // The parameters of the GetRecentFiles call.
    const Params params;

    // The callback to be called once all files are gathered. We do not know
    // ahead of time when this may be the case, due to nested directories.
    // Thus this class behaves similarly to a Barrier class, except that the
    // number of times the barrier has to be called varies.
    GetRecentFilesCallback callback;

    // Time when this call started.
    base::TimeTicks build_start_time;

    // Number of in-flight ReadDirectory() calls by ScanDirectory().
    int num_inflight_readdirs = 0;

    // Maps a document ID to a RecentFile. In OnGotRecentDocuments(), this map
    // is initialized with document IDs returned by GetRecentDocuments(), and
    // its values are filled as we scan the tree in ScanDirectory().
    // In case of multiple files with the same document ID found, the file with
    // lexicographically smallest URL is kept. A nullopt value means the
    // corresponding file is not (yet) found.
    std::map<std::string, std::optional<RecentFile>> document_id_to_file;
  };

  // Returns whether or not this scanner supports files with the given type.
  bool MatchesFileType(FileType file_type) const;

  // Extra method that allows us to insert an optional lag between the runner
  // being done and the OnGotRecentDocuments being called.
  void OnRunnerDone(
      const int32_t call_id,
      std::optional<std::vector<arc::mojom::DocumentPtr>> maybe_documents);

  // The method called once recent document pointers have been retrieved. This
  // may take place immediately after the runner was done, or with a small lag
  // that helps testing the interaction with the Stop method.
  void OnGotRecentDocuments(
      const int32_t call_id,
      std::optional<std::vector<arc::mojom::DocumentPtr>> maybe_documents);

  // Starts scanning of the directory with the given path.
  void ScanDirectory(const int32_t call_id, const base::FilePath& path);

  // The method called once a scan of directory is completed.
  void OnDirectoryRead(
      const int32_t call_id,
      const base::FilePath& path,
      base::File::Error result,
      std::vector<arc::ArcDocumentsProviderRoot::ThinFileInfo> files);

  // Invoked once traversing of the directory hirerachy is finished.
  void OnComplete(const int32_t call_id);

  // Creates a complete FileSystemURL for the given `path`, with the
  // help of `relative_mount_path_`.
  storage::FileSystemURL BuildDocumentsProviderUrl(
      const Params& params,
      const base::FilePath& path) const;

  bool WillArcFileSystemOperationsRunImmediately();

  // A map from the call ID to the call context.
  base::IDMap<std::unique_ptr<CallContext>> context_map_;

  // The profile for which this recent source was created.
  const raw_ptr<Profile> profile_;

  // The ARC root, such as Documents, Images, Media or Audio.
  const std::string root_id_;

  // The path at which the given ARC system is mounted.
  const base::FilePath relative_mount_path_;

  // The artificial lag introduced to this root for test purposes.
  base::TimeDelta lag_;

  // Timer; only allocated if the lag is positive.
  std::unique_ptr<base::OneShotTimer> timer_;

  base::WeakPtrFactory<RecentArcMediaSource> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_RECENT_ARC_MEDIA_SOURCE_H_
