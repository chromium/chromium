// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for ARC documents provider file system.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_UTIL_H_

#include <string>
#include <vector>

#include "ash/components/arc/mojom/file_system.mojom-forward.h"
#include "base/files/file_path.h"

class GURL;

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace arc {

// The name of ARC documents provider file system mount point.
extern const char kDocumentsProviderMountPointName[];

// The path of ARC documents provider file system mount point.
extern const base::FilePath::CharType kDocumentsProviderMountPointPath[];

// MIME type for directories in Android.
// Defined as DocumentsContract.Document.MIME_TYPE_DIR in Android.
extern const char kAndroidDirectoryMimeType[];

// Escapes a string so it can be used as a file/directory name.
// [%/.] are escaped with percent-encoding.
// NOTE: This function is visible only for unit testing. Usually you should not
// call this function directly.
std::string EscapePathComponent(const std::string& name);

// Unescapes a string escaped by EscapePathComponent().
// NOTE: This function is visible only for unit testing. Usually you should not
// call this function directly.
std::string UnescapePathComponent(const std::string& escaped);

// Returns the path of a directory where the specified DocumentsProvider is
// mounted.
// Appropriate escaping is done to embed |authority| and |root_document_id| in
// a file path.
base::FilePath GetDocumentsProviderMountPath(const std::string& authority,
                                             const std::string& root_id);

// Returns the "escaped_authority/escaped_root_id" suffix of the
// "/foo/bar/baz/escaped_authority/escaped_root_id" that is returned
// by GetDocumentsProviderMountPath.
base::FilePath GetDocumentsProviderMountPathSuffix(const std::string& authority,
                                                   const std::string& root_id);

// Parses an absolute file |path| from the ARC documents provider file system.
// Appropriate unescaping is done to extract |authority| and |root_id|
// from |path|.
// On success, true is returned. All arguments must not be nullptr.
bool ParseDocumentsProviderPath(const base::FilePath& path,
                                std::string* authority,
                                std::string* root_id);

// Parses a FileSystem URL pointing to ARC documents provider file system.
// Appropriate unescaping is done to extract |authority| and |root_id|
// from |url|.  The absolute file |path| is returned with appropriate escaping.
// On success, true is returned. All arguments must not be nullptr.
bool ParseDocumentsProviderUrl(const storage::FileSystemURL& url,
                               std::string* authority,
                               std::string* root_id,
                               base::FilePath* path);

// C++ implementation of DocumentsContract.buildDocumentUri() in Android.
GURL BuildDocumentUrl(const std::string& authority,
                      const std::string& document_id);

// Similar to net::GetExtensionsForMimeType(), but this covers more MIME types
// used in Android.
// Returns an empty vector if the MIME type is not known.
// If the returned vector is not empty, the first extension is the preferred
// extension.
std::vector<base::FilePath::StringType> GetExtensionsForArcMimeType(
    const std::string& mime_type);

// Computes a file name for a document.
base::FilePath::StringType GetFileNameForDocument(
    const mojom::DocumentPtr& document);

// Returns the provided MIME type without the subtype component.
std::string StripMimeSubType(const std::string& mime_type);

// Finds the first matching mime type with |ext| as a valid extension from the
// internal list of Android mime types. On success, the first matching MIME type
// is returned. On failure, nullptr is returned.
std::string FindArcMimeTypeFromExtension(const std::string& ext);

// Returns an ID of a Documents Provider volume.
std::string GetDocumentsProviderVolumeId(const std::string& authority,
                                         const std::string& root_id);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_UTIL_H_
