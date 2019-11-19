// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_UTIL_H_

#include "base/callback_forward.h"
#include "storage/common/file_system/file_system_types.h"

class GURL;
class Profile;

namespace base {
class FilePath;
}

namespace storage {
class FileSystemURL;
}

namespace chromeos {

// Returns whether the external file URL is provided for the |type| or not.
// TODO(b/119597913): Remove |force| from all functions in this file
// once ARC++ can access FUSE-mounted filesystems directly.
bool IsExternalFileURLType(storage::FileSystemType type, bool force = false);

// Obtains the external file url formatted as "externalfile:<path>" from file
// path. Returns empty URL if the file system does not provide the external file
// URL.
GURL FileSystemURLToExternalFileURL(
    const storage::FileSystemURL& file_system_url,
    bool force = false);

// Converts a externalfile: URL back to a virtual path of FileSystemURL.
base::FilePath ExternalFileURLToVirtualPath(const GURL& url);

// Converts a virtual path of FileSystemURL to an externalfile: URL.
GURL VirtualPathToExternalFileURL(const base::FilePath& virtual_path);

// Obtains external file URL (e.g. external:drive/root/sample.txt) from file
// path (e.g. /special/drive-xxx/root/sample.txt), if the |path| points an
// external location (drive, MTP, or FSP). Otherwise, it returns empty URL.
GURL CreateExternalFileURLFromPath(Profile* profile,
                                   const base::FilePath& path,
                                   bool force = false);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_UTIL_H_
