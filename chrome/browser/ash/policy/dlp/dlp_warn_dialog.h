// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_

#include <vector>

#include "base/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace policy {

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
  // recommended. Calls one of |accept_callback| or |cancel_callback| based on
  // the user's choice whether to proceed or not.
  static void ShowDlpPrintWarningDialog(base::OnceClosure accept_callback,
                                        base::OnceClosure cancel_callback);

  // Shows a warning dialog that informs the user that screen capture is not
  // recommended due to |confidential_web_contents| visible. Calls one of
  // |accept_callback| or |cancel_callback| based on the user's choice whether
  // to proceed or not.
  static void ShowDlpScreenCaptureWarningDialog(
      base::OnceClosure accept_callback,
      base::OnceClosure cancel_callback,
      const std::vector<content::WebContents*>& confidential_web_contents);

  // Shows a warning dialog that informs the user that video capture is not
  // recommended due to |confidential_web_contents| visible. Calls one of
  // |accept_callback| or |cancel_callback| based on the user's choice whether
  // to proceed or not.
  static void ShowDlpVideoCaptureWarningDialog(
      base::OnceClosure accept_callback,
      base::OnceClosure cancel_callback,
      const std::vector<content::WebContents*>& confidential_web_contents);

  DlpWarnDialog(const DlpWarnDialog&) = delete;
  DlpWarnDialog& operator=(const DlpWarnDialog&) = delete;
  ~DlpWarnDialog() override = default;

 private:
  // Helper method to create and show a warning dialog for a given
  // |restriction|.
  static void ShowDlpWarningDialog(
      base::OnceClosure accept_callback,
      base::OnceClosure cancel_callback,
      Restriction restriction,
      const std::vector<content::WebContents*>& confidential_web_contents);

  DlpWarnDialog(
      base::OnceClosure accept_callback,
      base::OnceClosure cancel_callback,
      Restriction restriction,
      const std::vector<content::WebContents*>& confidential_web_contents);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_
