// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_ARC_MEDIA_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_ARC_MEDIA_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/fileapi/recent_source.h"

class Profile;

namespace chromeos {

class RecentFile;

// RecentSource implementation for ARC media view.
//
// All member functions must be called on the UI thread.
class RecentArcMediaSource : public RecentSource {
 public:
  explicit RecentArcMediaSource(Profile* profile);
  ~RecentArcMediaSource() override;

  // RecentSource overrides:
  void GetRecentFiles(Params params) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(RecentArcMediaSourceTest, UmaStats);
  FRIEND_TEST_ALL_PREFIXES(RecentArcMediaSourceTest, UmaStats_Deferred);

  class MediaRoot;

  static const char kLoadHistogramName[];

  void OnGetRecentFilesForRoot(std::vector<RecentFile> files);
  void OnComplete();

  bool WillArcFileSystemOperationsRunImmediately();

  Profile* const profile_;
  std::vector<std::unique_ptr<MediaRoot>> roots_;

  base::Optional<Params> params_;

  // Time when the build started.
  base::TimeTicks build_start_time_;

  int num_inflight_roots_ = 0;
  std::vector<RecentFile> files_;

  base::WeakPtrFactory<RecentArcMediaSource> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RecentArcMediaSource);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_ARC_MEDIA_SOURCE_H_
