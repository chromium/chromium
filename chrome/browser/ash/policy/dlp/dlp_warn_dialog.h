// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_

#include <string>
#include "base/callback_forward.h"
#include "chrome/browser/ash/policy/dlp/dlp_confidential_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  enum class Restriction {
    kScreenCapture,
    kVideoCapture,
    kPrinting,
    kScreenShare
  };

  // A structure to keep track of optional and configurable parameters of a
  // DlpWarnDialog.
  struct DlpWarnDialogOptions {
    DlpWarnDialogOptions() = delete;
    explicit DlpWarnDialogOptions(Restriction restriction);
    DlpWarnDialogOptions(Restriction restriction,
                         DlpConfidentialContents confidential_contents);
    DlpWarnDialogOptions(Restriction restriction,
                         DlpConfidentialContents confidential_contents,
                         const std::u16string& application_title);
    DlpWarnDialogOptions(const DlpWarnDialogOptions& other);
    DlpWarnDialogOptions& operator=(const DlpWarnDialogOptions& other);
    ~DlpWarnDialogOptions();

    Restriction restriction;
    DlpConfidentialContents confidential_contents;
    absl::optional<std::u16string> application_title;
  };

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

  // Shows a warning dialog that informs the user that screen sharing is not
  // recommended due to |confidential_contents| visible. Calls |callback| and
  // passes user's choice of whether to proceed or not.
  static void ShowDlpScreenShareWarningDialog(
      OnDlpRestrictionChecked callback,
      const DlpConfidentialContents& confidential_contents,
      const std::u16string& application_title);

  DlpWarnDialog& operator=(const DlpWarnDialog&) = delete;
  ~DlpWarnDialog() override = default;

 private:
  // Helper method to create and show a warning dialog for a given
  // |restriction|.
  static void ShowDlpWarningDialog(OnDlpRestrictionChecked callback,
                                   DlpWarnDialogOptions options);

  DlpWarnDialog(OnDlpRestrictionChecked callback, DlpWarnDialogOptions options);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_
