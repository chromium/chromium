// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_ACCESS_CLOUD_IDENTIFIER_CLOUD_IDENTIFIER_UTIL_ASH_H_
#define CHROME_BROWSER_FILE_SYSTEM_ACCESS_CLOUD_IDENTIFIER_CLOUD_IDENTIFIER_UTIL_ASH_H_

#include "chrome/browser/ash/crosapi/file_system_access_cloud_identifier_provider_ash.h"
#include "chromeos/crosapi/mojom/file_system_access_cloud_identifier.mojom.h"

namespace base {
class FilePath;
}

namespace cloud_identifier {

// Ash-chrome specific implementation that retrieves the cloud identifiers for a
// given file from DriveFS or provided file systems.
void GetCloudIdentifier(const base::FilePath& url,
                        crosapi::mojom::HandleType handle_type,
                        crosapi::FileSystemAccessCloudIdentifierProviderAsh::
                            GetCloudIdentifierCallback callback);
}  // namespace cloud_identifier

#endif  // CHROME_BROWSER_FILE_SYSTEM_ACCESS_CLOUD_IDENTIFIER_CLOUD_IDENTIFIER_UTIL_ASH_H_
