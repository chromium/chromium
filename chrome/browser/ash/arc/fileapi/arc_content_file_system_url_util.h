// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_URL_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_URL_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "url/gurl.h"

namespace storage {
class FileSystemURL;
}

namespace arc {

// The name of the ARC content file system mount point.
extern const char kContentFileSystemMountPointName[];

// The path of the ARC content file system mount point.
extern const base::FilePath::CharType kContentFileSystemMountPointPath[];

// Escapes the given ARC URL.
std::string EscapeArcUrl(const GURL& arc_url);

// Unescapes the given escaped ARC URL.
GURL UnescapeArcUrl(const std::string& escaped_arc_url);

// Converts a URL which can be used within the ARC container to an externalfile:
// URL which can be used by Chrome.
GURL ArcUrlToExternalFileUrl(const GURL& arc_url);

// Converts an externalfile: URL to a URL which can be used within the ARC
// container. If the given URL cannot be converted to an ARC URL, returns an
// empty GURL.
GURL ExternalFileUrlToArcUrl(const GURL& external_file_url);

// Converts a FileSystemURL to a URL which can be used within the ARC container.
GURL FileSystemUrlToArcUrl(const storage::FileSystemURL& url);

// Converts a path which was returned by FileSystemURL::path() (not to be
// confused with virtual_path()) to a URL which can be used within the ARC
// container. If the given path is not under the ARC content file system mount
// point, returns an empty GURL.
GURL PathToArcUrl(const base::FilePath& path);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_URL_UTIL_H_
