// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OPEN_DIALOG_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OPEN_DIALOG_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace content {
class WebContents;
}  // namespace content

// Creates and shows the open confirmation dialog.
void ShowDownloadOpenConfirmationDialog(
    content::WebContents* web_contents,
    const std::string& extension_name,
    const base::FilePath& file_path,
    base::OnceCallback<void(bool)> open_callback);

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OPEN_DIALOG_H_
