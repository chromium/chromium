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

using AuthorizationCompletedCallback =
    FileSystemSigninDialogDelegate::AuthorizationCompletedCallback;

void StartSigninExperienceForDownloadItem(
    content::WebContents* web_contents,
    const FileSystemSettings& settings,
    AuthorizationCompletedCallback callback);

void ReturnCancellation(AuthorizationCompletedCallback callback);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_EXPERIENCE_H_
