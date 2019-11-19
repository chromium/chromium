// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_UTILS_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/download/android/download_open_source.h"

namespace download {
class DownloadItem;
}

class GURL;

// Native side of DownloadUtils.java.
class DownloadUtils {
 public:
  static base::FilePath GetUriStringForPath(const base::FilePath& file_path);
  static int GetAutoResumptionSizeLimit();
  static void OpenDownload(download::DownloadItem* item,
                           DownloadOpenSource open_source);
  static std::string RemapGenericMimeType(const std::string& mime_type,
                                          const GURL& url,
                                          const std::string& file_name);
  static bool ShouldAutoOpenDownload(download::DownloadItem* item);
  static bool IsOmaDownloadDescription(const std::string& mime_type);

  // Called to show the download manager, with a choice to focus on prefetched
  // content instead of regular downloads. |download_open_source| is the source
  // of the action.
  static void ShowDownloadManager(bool show_prefetched_content,
                                  DownloadOpenSource open_source);
  static bool IsDownloadUserInitiated(download::DownloadItem* download);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_UTILS_H_
