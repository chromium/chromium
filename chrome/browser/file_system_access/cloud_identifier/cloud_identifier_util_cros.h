// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_ACCESS_CLOUD_IDENTIFIER_CLOUD_IDENTIFIER_UTIL_CROS_H_
#define CHROME_BROWSER_FILE_SYSTEM_ACCESS_CLOUD_IDENTIFIER_CLOUD_IDENTIFIER_UTIL_CROS_H_

#include "chromeos/crosapi/mojom/file_system_access_cloud_identifier.mojom-forward.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/file_system_access_permission_context.h"

namespace storage {
class FileSystemURL;
}

namespace cloud_identifier {

void SetCloudIdentifierProviderForTesting(
    crosapi::mojom::FileSystemAccessCloudIdentifierProvider* provider);

// CrOS-specific implementation to retrieve cloud identifiers for files and
// directories, which will reach out to
// `FileSystemAccessCloudIdentifierProviderAsh` in ash-chrome via cros-api for
// cloud-syncable file systems (e.g. DriveFS or provided FS).
void GetCloudIdentifierFromAsh(
    const storage::FileSystemURL& url,
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    content::ContentBrowserClient::GetCloudIdentifiersCallback callback);

}  // namespace cloud_identifier

#endif  // CHROME_BROWSER_FILE_SYSTEM_ACCESS_CLOUD_IDENTIFIER_CLOUD_IDENTIFIER_UTIL_CROS_H_
