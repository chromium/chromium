// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UI_MODEL_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UI_MODEL_UTILS_H_

#include "base/time/time.h"
#include "chrome/browser/download/download_ui_model.h"

// Whether the download is more recent than |cutoff_time|.
bool DownloadUIModelIsRecent(const DownloadUIModel* model,
                             base::Time cutoff_time);

// Whether the download is in progress and pending deep scanning.
bool IsPendingDeepScanning(const DownloadUIModel* model);

// Whether the download is considered in-progress from the UI's point of view.
// Consider dangerous downloads as completed, because we don't want to encourage
// users to interact with them. However, consider downloads pending scanning as
// in progress, because we do want users to scan potential dangerous downloads.
bool IsModelInProgress(const DownloadUIModel* model);

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UI_MODEL_UTILS_H_
