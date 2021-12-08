// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_WARN_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_WARN_NOTIFIER_H_

#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_dialog.h"

namespace policy {

// DlpWarnNotifier is used to create and show DlpWarnDialogs and should be the
// only way to do this.
class DlpWarnNotifier {
 public:
  DlpWarnNotifier() = default;
  DlpWarnNotifier(const DlpWarnNotifier& other) = delete;
  DlpWarnNotifier& operator=(const DlpWarnNotifier& other) = delete;
  virtual ~DlpWarnNotifier() = default;

  // Shows a warning dialog that informs the user that printing is not
  // recommended. Calls |callback| and passes user's choice of whether to
  // proceed or not.
  void ShowDlpPrintWarningDialog(
      OnDlpRestrictionCheckedCallback callback) const;

  // Shows a warning dialog that informs the user that screen capture is not
  // recommended due to |confidential_contents| visible. Calls |callback| and
  // passes user's choice of whether to proceed or not.
  void ShowDlpScreenCaptureWarningDialog(
      OnDlpRestrictionCheckedCallback callback,
      const DlpConfidentialContents& confidential_contents) const;

  // Shows a warning dialog that informs the user that video capture is not
  // recommended due to |confidential_contents| visible. Calls |callback| and
  // passes user's choice of whether to proceed or not.
  void ShowDlpVideoCaptureWarningDialog(
      OnDlpRestrictionCheckedCallback callback,
      const DlpConfidentialContents& confidential_contents) const;

  // Shows a warning dialog that informs the user that screen sharing is not
  // recommended due to |confidential_contents| visible. Calls |callback| and
  // passes user's choice of whether to proceed or not.
  void ShowDlpScreenShareWarningDialog(
      OnDlpRestrictionCheckedCallback callback,
      const DlpConfidentialContents& confidential_contents,
      const std::u16string& application_title) const;

 private:
  // Helper method to create and show a warning dialog for a given
  // |restriction|.
  virtual void ShowDlpWarningDialog(
      OnDlpRestrictionCheckedCallback callback,
      DlpWarnDialog::DlpWarnDialogOptions options) const;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_WARN_NOTIFIER_H_
