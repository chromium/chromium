// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_EXPERIENCE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_EXPERIENCE_H_

#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/file_system/signin_dialog_delegate.h"

namespace content {
class WebContents;
}

namespace enterprise_connectors {

// Retrieve FileSystemSettings for |profile|; null if connector is disabled.
absl::optional<FileSystemSettings> GetFileSystemSettings(Profile* profile);

// Retrieve FileSystemSettings for |download_item|, considering its associated
// profile and originating URL; null if connector is disabled.
absl::optional<FileSystemSettings> GetFileSystemSettings(
    download::DownloadItem* download_item);

using AuthorizationCompletedCallback =
    FileSystemSigninDialogDelegate::AuthorizationCompletedCallback;

// Start the sign in experience as triggered by a download item.
void StartFileSystemConnectorSigninExperienceForDownloadItem(
    content::WebContents* web_contents,
    const FileSystemSettings& settings,
    AuthorizationCompletedCallback callback);

// Run |callback| with a GoogleServiceAuthError that indicates cancellation.
void ReturnCancellation(AuthorizationCompletedCallback callback);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_EXPERIENCE_H_
