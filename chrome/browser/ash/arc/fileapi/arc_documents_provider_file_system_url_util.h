// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_FILE_SYSTEM_URL_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_FILE_SYSTEM_URL_UTIL_H_

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "storage/browser/file_system/file_system_url.h"

namespace arc {

// Converts a FilesystemURL to ARC content URL on the IO thread.
void ResolveToContentUrlOnIOThread(
    const storage::FileSystemURL& url,
    ArcDocumentsProviderRoot::ResolveToContentUrlCallback callback);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_FILE_SYSTEM_URL_UTIL_H_
