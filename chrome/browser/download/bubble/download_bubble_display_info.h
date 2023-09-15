// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_DISPLAY_INFO_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_DISPLAY_INFO_H_

#include "base/time/time.h"

// Summarizes information from DownloadUIModels to be displayed in the download
// bubble. Produced by DownloadBubbleUpdateService by iterating through models,
// which is somewhat expensive, so summarized information is stored in the form
// of this struct.
struct DownloadBubbleDisplayInfo {
  // Number of models that would be returned to display.
  size_t all_models_size = 0;
  // The last time that a download was completed. Will be null if no downloads
  // were completed.
  base::Time last_completed_time;
  // Whether there are any downloads actively doing deep scanning.
  bool has_deep_scanning = false;
  // Whether any downloads are unactioned.
  bool has_unactioned = false;
  // From the button UI's perspective, whether the download is considered in
  // progress. Consider dangerous downloads as completed, because we don't
  // want to encourage users to interact with them.
  int in_progress_count = 0;
  // Count of in-progress downloads (by the above definition) that are paused.
  int paused_count = 0;

  // Returns a reference to a singleton empty struct. This is for callers who
  // return references but don't have anything to return in some cases.
  static const DownloadBubbleDisplayInfo& EmptyInfo();
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_DISPLAY_INFO_H_
