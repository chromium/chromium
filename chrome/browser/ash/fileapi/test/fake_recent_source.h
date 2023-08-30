// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_TEST_FAKE_RECENT_SOURCE_H_
#define CHROME_BROWSER_ASH_FILEAPI_TEST_FAKE_RECENT_SOURCE_H_

#include <vector>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/fileapi/recent_source.h"

namespace ash {

class RecentFile;

// Fake implementation of RecentSource that returns a canned set of files.
//
// All member functions must be called on the UI thread.
class FakeRecentSource : public RecentSource {
 public:
  FakeRecentSource();

  FakeRecentSource(const FakeRecentSource&) = delete;
  FakeRecentSource& operator=(const FakeRecentSource&) = delete;

  ~FakeRecentSource() override;

  // Add a file to the canned set.
  void AddFile(const RecentFile& file);

  // Adds artificial lag to the source.
  void SetLag(const base::TimeDelta& lag);

  // RecentSource overrides:
  void GetRecentFiles(Params params) override;

 private:
  // Invoked by OneShotTimer after the lag has passed.
  void OnFilesReady(Params params);

  // Returns a vector of matching files.
  std::vector<RecentFile> GetMatchingFiles(const Params& params);

  // Returns true if the file matches the given file type.
  bool MatchesFileType(const RecentFile& file,
                       RecentSource::FileType file_type) const;

  std::vector<RecentFile> canned_files_;

  // The delay with which the response to GetRecentFiles is posted.
  base::TimeDelta lag_;

  // The timer used to trigger the response.
  base::OneShotTimer timer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_TEST_FAKE_RECENT_SOURCE_H_
