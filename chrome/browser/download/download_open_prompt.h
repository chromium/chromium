// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OPEN_PROMPT_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OPEN_PROMPT_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace content {
class WebContents;
}

// Prompts the user for whether to open a DownloadItem using native UI. This
// step is necessary to prevent a malicious extension from opening any
// downloaded file.
class DownloadOpenPrompt {
 public:
  using OpenCallback = base::OnceCallback<void(bool /* accept */)>;

  // Creates the open confirmation dialog and returns this object.
  static DownloadOpenPrompt* CreateDownloadOpenConfirmationDialog(
      content::WebContents* web_contents,
      const std::string& extension_name,
      const base::FilePath& file_path,
      OpenCallback open_callback);

  DownloadOpenPrompt(const DownloadOpenPrompt&) = delete;
  DownloadOpenPrompt& operator=(const DownloadOpenPrompt&) = delete;

  // Called to accept the confirmation dialog for testing.
  static void AcceptConfirmationDialogForTesting(
      DownloadOpenPrompt* download_danger_prompt);

 protected:
  DownloadOpenPrompt();
  ~DownloadOpenPrompt();
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OPEN_PROMPT_H_
