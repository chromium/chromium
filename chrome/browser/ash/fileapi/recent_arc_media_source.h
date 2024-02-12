// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_RECENT_ARC_MEDIA_SOURCE_H_
#define CHROME_BROWSER_ASH_FILEAPI_RECENT_ARC_MEDIA_SOURCE_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/id_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/fileapi/recent_source.h"

class Profile;

namespace ash {

class RecentFile;

// RecentSource implementation for ARC media view.
//
// All member functions must be called on the UI thread.
class RecentArcMediaSource : public RecentSource {
 public:
  // Creates a recent file sources that scans Arc media. The `profile` is used
  // to create scanners for all known media roots (Documents, Movies, etc.). The
  // `max_files` parameter limits the maximum number of files returned by this
  // source to the callback specified in the parameters of the GetRecentFiles
  // method.
  RecentArcMediaSource(Profile* profile, size_t max_files);

  RecentArcMediaSource(const RecentArcMediaSource&) = delete;
  RecentArcMediaSource& operator=(const RecentArcMediaSource&) = delete;

  ~RecentArcMediaSource() override;

  // Overrides the base class method to launch searches for audio, videos, image
  // and document files.
  void GetRecentFiles(Params params, GetRecentFilesCallback callback) override;

  // Overrides the base class Stop method to return partial results collected
  // before the timeout call. This method must be called on the UI thread.
  std::vector<RecentFile> Stop(int32_t call_id) override;

  // Causes laggy performance for the given `media_root`. This is to be only
  // used in tests. The media_root must be one of roots defined in
  // arc_media_view_util.h. The return value indicates if the lag was set.
  bool SetLagForTesting(const char* media_root, const base::TimeDelta& lag);

  // The name of the metric under which recent file access statistics for ARC
  // are recorded.
  static const char kLoadHistogramName[];

 private:
  // The class that handles a specific root of the ARC folder, such as
  // documents, videos, images.
  class MediaRoot;

  // Call context stores information specific to a single GetRecentFiles call.
  // If multiple calls are issued each will have its own context.
  struct CallContext {
    explicit CallContext(GetRecentFilesCallback callback);
    // Move constructor needed as callback cannot be copied.
    CallContext(CallContext&& context);
    ~CallContext();

    // The callback to be called once all files are gathered. We do not know
    // ahead of time when this may be the case, due to nested directories.
    // Thus this class behaves similarly to a Barrier class, except that the
    // number of times the barrier has to be called varies.
    GetRecentFilesCallback callback;

    // Time when this call started.
    base::TimeTicks build_start_time;

    // The set of media roots that have been asked to find matching files, but
    // have not yet returned its results.
    std::set<raw_ptr<MediaRoot>> active_roots;

    // The set of files collected so far.
    std::vector<RecentFile> files;
  };

  // The method called by each media root as it completes its search.
  void OnGotRecentFiles(const int32_t call_id,
                        MediaRoot* root,
                        std::vector<RecentFile> files);

  // The method called once all media roots are done.
  void OnComplete(const int32_t call_id);

  bool WillArcFileSystemOperationsRunImmediately();

  // A map from root ID to a media roots. Typically we use a root for each media
  // type: images, videos, documents and audio files.
  std::map<const char*, std::unique_ptr<MediaRoot>> roots_;

  // A map from the call ID to the call context.
  base::IDMap<std::unique_ptr<CallContext>> context_map_;

  // The profile for which this recent source was created.
  const raw_ptr<Profile> profile_;

  // The maximum number of files to be returned on the callback.
  const size_t max_files_;

  base::WeakPtrFactory<RecentArcMediaSource> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_RECENT_ARC_MEDIA_SOURCE_H_
