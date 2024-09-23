// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DANGER_PROMPT_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DANGER_PROMPT_H_

#include "base/functional/callback_forward.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace content {
class WebContents;
}

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

  // Return a new self-deleting DownloadDangerPrompt. The returned
  // DownloadDangerPrompt* is only used for testing. The caller does not own the
  // object and receives no guarantees about lifetime. The prompt message will
  // contain some information about the download and its danger. |done| is a
  // callback called when the ACCEPT, CANCEL or DISMISS action is invoked.
  // |done| may be called with the CANCEL action even when |item| is either no
  // longer dangerous or no longer in progress, or if the tab corresponding to
  // |web_contents| is closing.
  static DownloadDangerPrompt* Create(download::DownloadItem* item,
                                      content::WebContents* web_contents,
                                      OnDone done);

  // Only to be used by tests. Subclasses must override to manually call the
  // respective button click handler.
  virtual void InvokeActionForTesting(Action action) = 0;

 protected:
  // Records warning action event consumed by Safe Browsing reports.
  static void RecordDownloadWarningEvent(Action action,
                                         download::DownloadItem* download);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_DANGER_PROMPT_H_
