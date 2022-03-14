// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_WARN_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_WARN_DIALOG_H_

#include <string>

#include "base/callback_forward.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace policy {

// The callback function that is invoked when the user addresses the
// DlpWarnDialog. When `should_proceed` is set to true, the action will continue
// as if there was no restricted content. Otherwise, the operation is aborted.
using OnDlpRestrictionCheckedCallback =
    base::OnceCallback<void(bool should_proceed)>;

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

  DlpWarnDialog() = delete;
  DlpWarnDialog(OnDlpRestrictionCheckedCallback callback,
                DlpWarnDialogOptions options);
  DlpWarnDialog(const DlpWarnDialog& other) = delete;
  DlpWarnDialog& operator=(const DlpWarnDialog& other) = delete;
  ~DlpWarnDialog() override = default;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_WARN_DIALOG_H_
