// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DANGER_PROMPT_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DANGER_PROMPT_H_

#include "base/functional/callback_forward.h"

namespace download {
class DownloadItem;
}

// Prompts the user for whether to Keep a dangerous DownloadItem using native
// UI. Having a native dialog intervene during the this workflow means that the
// extension renderer no longer has the privilege to accept a dangerous download
// from script without user intervention. This step is necessary to prevent a
// malicious script form abusing such a privilege. This is only used for
// extensions API downloads.
class DownloadDangerPrompt {
 public:
  // Actions resulting from showing the danger prompt.
  enum Action {
    // The user chose to proceed down the dangerous path.
    ACCEPT,
    // The user chose not to proceed down the dangerous path.
    CANCEL,
    // The user dismissed the dialog without making an explicit choice.
    DISMISS,
  };
  typedef base::OnceCallback<void(Action)> OnDone;

  // Records warning action event consumed by Safe Browsing reports.
  static void RecordDownloadWarningEvent(Action action,
                                         download::DownloadItem* download);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DANGER_PROMPT_H_
