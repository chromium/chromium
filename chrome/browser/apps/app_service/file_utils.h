// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_FILE_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_FILE_UTILS_H_

#include <vector>

class Profile;
class GURL;

namespace base {
class FilePath;
}

namespace storage {
class FileSystemURL;
}

namespace apps {
std::vector<storage::FileSystemURL> GetFileSystemURL(
    Profile* profile,
    const std::vector<GURL>& file_urls);

// Convert absolute file path to filesystem: scheme url.
std::vector<GURL> GetFileUrls(Profile* profile,
                              const std::vector<base::FilePath>& file_paths);
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_FILE_UTILS_H_
