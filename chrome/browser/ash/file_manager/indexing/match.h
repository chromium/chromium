// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_MATCH_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_MATCH_H_

#include "chrome/browser/ash/file_manager/indexing/file_info.h"

namespace file_manager {

// Represents a single search match. The match consists of the file info, which
// identified the matched file and match score. The match score is a value
// between 0 and 1, indicating how good the match is. The score of 1 means a
// perfect match. Score 0 indicates a non-match.
struct Match {
  Match(float score, const FileInfo& file_info);
  ~Match();

  // Returns whether this Match is equal to the `other` Match.
  bool operator==(const Match& other) const;

  // The score for this match.
  float score;

  // FileInfo of the matched file.
  FileInfo file_info;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_MATCH_H_
