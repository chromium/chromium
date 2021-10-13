// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_

#include "base/callback_forward.h"
#include "chrome/browser/ash/policy/dlp/dlp_confidential_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace policy {

using OnDlpRestrictionChecked = base::OnceCallback<void(bool should_proceed)>;

// DlpWarnDialog is a system modal dialog shown when Data Leak Protection on
// screen restriction (Screen Capture, Printing, Screen Share) level is set to
// WARN.
class DlpWarnDialog : public views::DialogDelegateView {
 public:
  METADATA_HEADER(DlpWarnDialog);

  // Type of the restriction for which the dialog is created, used to determine
  // the text shown in the dialog.
  enum class Restriction { kScreenCapture, kVideoCapture, kPrinting };

  // Shows a warning dialog that informs the user that printing is not
  // recommended. Calls |callback| and passes user's choice of whether to
  // proceed or not.
  static void ShowDlpPrintWarningDialog(OnDlpRestrictionChecked callback);

  // Shows a warning dialog that informs the user that screen capture is not
  // recommended due to |confidential_contents| visible. Calls |callback| and
  // passes user's choice of whether to proceed or not.
  static void ShowDlpScreenCaptureWarningDialog(
      OnDlpRestrictionChecked callback,
      const DlpConfidentialContents& confidential_contents);

  // Shows a warning dialog that informs the user that video capture is not
  // recommended due to |confidential_contents| visible. Calls |callback| and
  // passes user's choice of whether to proceed or not.
  static void ShowDlpVideoCaptureWarningDialog(
      OnDlpRestrictionChecked callback,
      const DlpConfidentialContents& confidential_contents);
  DlpWarnDialog& operator=(const DlpWarnDialog&) = delete;
  ~DlpWarnDialog() override = default;

 private:
  // Helper method to create and show a warning dialog for a given
  // |restriction|.
  static void ShowDlpWarningDialog(
      OnDlpRestrictionChecked callback,
      Restriction restriction,
      const DlpConfidentialContents& confidential_contents);

  DlpWarnDialog(OnDlpRestrictionChecked callback,
                Restriction restriction,
                const DlpConfidentialContents& confidential_contents);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_
