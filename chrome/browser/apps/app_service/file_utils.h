// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_FILE_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_FILE_UTILS_H_

#include <vector>

class GURL;

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace storage {
class FileSystemURL;
}

namespace apps {
// Convert a list of filesystem: scheme url to a list of FileSystemURL.
// The returned FileSystemURL could be invalid.
std::vector<storage::FileSystemURL> GetFileSystemURL(
    content::BrowserContext* browser_context,
    const std::vector<GURL>& file_urls);

// Convert filesystem: scheme url to FileSystemURL. The returned FileSystemURL
// could be invalid.
storage::FileSystemURL GetFileSystemURL(
    content::BrowserContext* browser_context,
    const GURL& file_url);

// Convert a list of absolute file path to a list of filesystem: scheme url.
std::vector<GURL> GetFileSystemUrls(
    content::BrowserContext* browser_context,
    const std::vector<base::FilePath>& file_paths);

// Convert absolute file path to filesystem: scheme url. Will return empty
// GURL if cannot get the filesystem: scheme url.
GURL GetFileSystemUrl(content::BrowserContext* browser_context,
                      const base::FilePath& file_path);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_FILE_UTILS_H_
