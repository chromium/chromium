// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_DIALOG_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_DIALOG_UTILS_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "components/download/public/common/download_item.h"
#include "url/gurl.h"

// Helper class for download dialogs.
class DownloadDialogUtils {
 public:
  // Helper method to find a download from a list of downloads based on its
  // GUID, and remove it from the list.
  static download::DownloadItem* FindAndRemoveDownload(
      std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>*
          downloads,
      const std::string& download_guid);

  // Called when a new file was created and inform |callback| about
  // the result and the new path.
  static void CreateNewFileDone(
      DownloadTargetDeterminerDelegate::ConfirmationCallback callback,
      download::PathValidationResult result,
      const base::FilePath& target_path);

  // Called to get an elided URL for a page URL, so that it can be displayed
  // on duplicate inforbar or dialog.
  static std::string GetDisplayURLForPageURL(const GURL& page_url);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_DIALOG_UTILS_H_
