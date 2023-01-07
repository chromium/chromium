// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_ICON_EXTRACTOR_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_ICON_EXTRACTOR_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/browser/icon_loader.h"

// Helper class for DownloadsGetFileIconFunction. Only used for a single icon
// extraction.
class DownloadFileIconExtractor {
 public:
  // Callback for |ExtractIconForPath|. The parameter is a URL as a string for a
  // suitable icon. The string could be empty if the icon could not be
  // determined.
  typedef base::OnceCallback<void(const std::string&)> IconURLCallback;

  virtual ~DownloadFileIconExtractor() {}

  // Should return false if the request was invalid.  If the return value is
  // true, then |callback| should be called with the result.
  virtual bool ExtractIconURLForPath(const base::FilePath& path,
                                     float scale,
                                     IconLoader::IconSize icon_size,
                                     IconURLCallback callback) = 0;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_ICON_EXTRACTOR_H_
