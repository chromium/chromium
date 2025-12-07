// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_ACCESS_CLOUD_IDENTIFIER_CLOUD_IDENTIFIER_UTIL_ASH_H_
#define CHROME_BROWSER_FILE_SYSTEM_ACCESS_CLOUD_IDENTIFIER_CLOUD_IDENTIFIER_UTIL_ASH_H_

#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/file_system_access_permission_context.h"

namespace storage {
class FileSystemURL;
}

namespace cloud_identifier {

// Ash-chrome specific implementation that retrieves the cloud identifiers for a
// given file from DriveFS or provided file systems.
void GetCloudIdentifier(
    const storage::FileSystemURL& url,
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    content::ContentBrowserClient::GetCloudIdentifiersCallback callback);
}  // namespace cloud_identifier

#endif  // CHROME_BROWSER_FILE_SYSTEM_ACCESS_CLOUD_IDENTIFIER_CLOUD_IDENTIFIER_UTIL_ASH_H_
